module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

namespace {
    struct AgentToolCallRequest {
        std::string id;
        std::string name;
        json arguments = json::object();
    };

    struct AgentProtocolResponse {
        std::string action;
        std::vector<AgentToolCallRequest> calls;
        json translations = json::array();
        json termUpdates = json::array();
        json rewriteRequests = json::array();
        json fileNotePatch = json::object();
        json summary = json::object();
        std::string rawContent;
    };

    std::string nowTimestampString() {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        return std::to_string(now);
    }

    json loadJsonFileOrDefault(const fs::path& path, const json& fallback = json::object()) {
        try {
            if (!fs::exists(path)) {
                return fallback;
            }
            std::ifstream ifs(path, std::ios::binary);
            return json::parse(ifs);
        }
        catch (...) {
            return fallback;
        }
    }

    void saveJsonFilePretty(const fs::path& path, const json& value) {
        createParent(path);
        std::ofstream ofs(path, std::ios::binary);
        ofs << value.dump(2);
    }

    fs::path buildAgentFileNotePath(const fs::path& root, const fs::path& relInputPath) {
        fs::path notePath = root / relInputPath;
        notePath += L".json";
        return notePath;
    }

    std::string trimCopy(std::string value) {
        const auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
        const auto begin = std::ranges::find_if(value, isNotSpace);
        if (begin == value.end()) {
            return {};
        }
        const auto end = std::ranges::find_if(value | std::views::reverse, isNotSpace).base();
        return std::string(begin, end);
    }

    std::optional<json> tryParseJsonEnvelope(std::string text) {
        text = trimCopy(std::move(text));
        if (text.empty()) {
            return std::nullopt;
        }

        const size_t fencedStart = text.find("```");
        if (fencedStart != std::string::npos) {
            const size_t lineEnd = text.find('\n', fencedStart);
            const size_t fencedEnd = text.rfind("```");
            if (lineEnd != std::string::npos && fencedEnd != std::string::npos && fencedEnd > lineEnd) {
                text = trimCopy(text.substr(lineEnd + 1, fencedEnd - lineEnd - 1));
            }
        }

        try {
            return json::parse(text);
        }
        catch (...) {}

        const size_t jsonStart = text.find('{');
        const size_t jsonEnd = text.rfind('}');
        if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
            return std::nullopt;
        }

        try {
            return json::parse(text.substr(jsonStart, jsonEnd - jsonStart + 1));
        }
        catch (...) {
            return std::nullopt;
        }
    }

    AgentProtocolResponse parseAgentTextResponse(const std::string& content) {
        AgentProtocolResponse result;
        result.rawContent = content;
        const std::optional<json> payloadOpt = tryParseJsonEnvelope(content);
        if (!payloadOpt.has_value() || !payloadOpt->is_object()) {
            throw std::runtime_error("Agent 响应不是合法 JSON 对象");
        }

        const json& payload = *payloadOpt;
        result.action = payload.value("action", "");
        if (const auto it = payload.find("calls"); it != payload.end() && it->is_array()) {
            for (const auto& call : *it) {
                if (!call.is_object()) {
                    continue;
                }
                AgentToolCallRequest parsed;
                parsed.id = call.value("id", std::format("call_{}", result.calls.size()));
                parsed.name = call.value("name", "");
                if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                    parsed.arguments = *argIt;
                }
                result.calls.push_back(std::move(parsed));
            }
        }
        if (const auto it = payload.find("translations"); it != payload.end() && it->is_array()) {
            result.translations = *it;
        }
        if (const auto it = payload.find("term_updates"); it != payload.end() && it->is_array()) {
            result.termUpdates = *it;
        }
        if (const auto it = payload.find("rewrite_requests"); it != payload.end() && it->is_array()) {
            result.rewriteRequests = *it;
        }
        if (const auto it = payload.find("file_note_patch"); it != payload.end() && it->is_object()) {
            result.fileNotePatch = *it;
        }
        if (const auto it = payload.find("summary"); it != payload.end() && it->is_object()) {
            result.summary = *it;
        }
        return result;
    }

    AgentProtocolResponse parseAgentApiResponse(const ApiResponse& response) {
        if (response.hasToolCalls) {
            AgentProtocolResponse result;
            result.action = "tool_calls";
            result.rawContent = response.content;
            for (const auto& toolCall : response.toolCalls) {
                if (!toolCall.is_object()) {
                    continue;
                }
                AgentToolCallRequest parsed;
                parsed.id = toolCall.value("id", std::format("tool_{}", result.calls.size()));
                if (const auto funcIt = toolCall.find("function"); funcIt != toolCall.end() && funcIt->is_object()) {
                    parsed.name = funcIt->value("name", "");
                    if (const auto argsIt = funcIt->find("arguments"); argsIt != funcIt->end()) {
                        if (argsIt->is_string()) {
                            const std::string argsStr = argsIt->get<std::string>();
                            if (const auto parsedArgs = tryParseJsonEnvelope(argsStr); parsedArgs.has_value()) {
                                parsed.arguments = *parsedArgs;
                            }
                        }
                        else {
                            parsed.arguments = *argsIt;
                        }
                    }
                }
                result.calls.push_back(std::move(parsed));
            }
            return result;
        }

        return parseAgentTextResponse(response.content);
    }

    bool shouldFallbackFromNativeFunctionCalling(const ApiResponse& response) {
        if (response.success) {
            return false;
        }
        std::string lower = response.content;
        str2LowerInplace(lower);
        return response.statusCode == 400 || response.statusCode == 404 || response.statusCode == 422
            || lower.contains("tool_choice")
            || lower.contains("tool_calls")
            || lower.contains("function")
            || lower.contains("tools");
    }

    json buildAgentNativeTools() {
        const json commonString = { {"type", "string"} };
        const json commonInt = { {"type", "integer"} };
        return json::array({
            {
                {"type", "function"},
                {"function", {
                    {"name", "list_files"},
                    {"description", "List available project files for cross-file lookup."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"pattern", commonString},
                            {"limit", commonInt}
                        }},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "read_lines"},
                    {"description", "Read a slice of source and cached translation lines from a file."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"file", commonString},
                            {"start", commonInt},
                            {"count", commonInt},
                            {"include_src", { {"type", "boolean"} }},
                            {"include_dst", { {"type", "boolean"} }}
                        }},
                        {"required", json::array({"file", "start", "count"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "search_text"},
                    {"description", "Search source text, cached translations, summaries, or term ledger."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", {
                            {"query", commonString},
                            {"scope", { {"type", "string"}, {"enum", json::array({"current_file", "all_files", "specified_file"})} }},
                            {"file", commonString},
                            {"limit", commonInt}
                        }},
                        {"required", json::array({"query", "scope"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "get_term"},
                    {"description", "Get the current term ledger record for a term."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", { {"term", commonString} }},
                        {"required", json::array({"term"})},
                        {"additionalProperties", false}
                    }}
                }}
            },
            {
                {"type", "function"},
                {"function", {
                    {"name", "get_file_note"},
                    {"description", "Read the saved summary and unresolved notes of a file."},
                    {"parameters", {
                        {"type", "object"},
                        {"properties", { {"file", commonString} }},
                        {"required", json::array({"file"})},
                        {"additionalProperties", false}
                    }}
                }}
            }
        });
    }

    int sanitizeToolLimit(int requested, int fallback, int maxLimit = 200) {
        if (requested <= 0) {
            return fallback;
        }
        return std::min(requested, maxLimit);
    }

    bool hasAgentWorkUnitArtifacts(
        const fs::path& relFilePath,
        const fs::path& outputDir,
        const fs::path& outputCacheDir,
        const fs::path& transCacheDir,
        bool needsCombining
    ) {
        const fs::path outputPath = needsCombining ? (outputCacheDir / relFilePath) : (outputDir / relFilePath);
        const fs::path cachePath = transCacheDir / relFilePath;
        return fs::exists(outputPath) && fs::exists(cachePath);
    }
}

NormalJsonTranslator::~NormalJsonTranslator() 
{
    m_logger->info("所有任务已完成！NormalJsonTranslator结束。");
}

NormalJsonTranslator::NormalJsonTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
                                           std::optional<fs::path> inputDir, std::optional<fs::path> inputCacheDir,
                                           std::optional<fs::path> outputDir, std::optional<fs::path> outputCacheDir) 
    :
    m_controller(controller), m_logger(logger), m_projectDir(projectDir),
    m_luaManager(std::make_unique<LuaManager>(logger)), m_pythonManager(std::make_unique<PythonManager>(logger))
{
    m_logger->info("GalTransl++ NormalJsonTranslator 启动...");
    m_inputDir = inputDir.value_or(m_projectDir / L"gt_input");
    m_inputCacheDir = inputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_input_cache");
    m_outputDir = outputDir.value_or(m_projectDir / L"gt_output");
    m_outputCacheDir = outputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_output_cache");
    m_transCacheDir = m_projectDir / transCacheDirName;
    m_otherCacheDir = m_projectDir / otherCacheDirName;
    m_backgroundTextCachePath = m_otherCacheDir / L"backgroundTextCache.json";
    m_agentRootDir = m_otherCacheDir / L"agent";
    m_agentRunStatePath = m_agentRootDir / L"run_state.json";
    m_agentTermLedgerPath = m_agentRootDir / L"term_ledger.json";
    m_agentRewriteQueuePath = m_agentRootDir / L"rewrite_queue.json";
    m_agentFileNotesDir = m_agentRootDir / L"file_notes";
    m_agentSearchCatalogPath = m_agentRootDir / L"search_catalog.json";
    try {
        if (fs::exists(m_backgroundTextCachePath)) {
            std::ifstream ifs(m_backgroundTextCachePath, std::ios::binary);
            json::parse(ifs).get_to(m_backgroundTextCacheMap);
        }
        else {
            m_logger->debug("未找到背景文本缓存 {}", wide2Ascii(m_backgroundTextCachePath));
        }
    }
    catch (...) {
        m_logger->error("读取背景文本缓存 {} 失败", wide2Ascii(m_backgroundTextCachePath));
    }
}

void NormalJsonTranslator::init()
{
    const fs::path configPath = m_projectDir / L"config.toml";
    try {
        
        const auto configData = toml::uparse(configPath);

        const std::string transEngineStr = toml::find_or(configData, "plugins", "transEngine", "");
        if (transEngineStr == "ForGalJson") {
            m_transEngine = TransEngine::ForGalJson;
        }
        else if (transEngineStr == "ForGalTsv") {
            m_transEngine = TransEngine::ForGalTsv;
        }
        else if (transEngineStr == "ForNovelTsv") {
            m_transEngine = TransEngine::ForNovelTsv;
        }
        else if (transEngineStr == "DeepseekJson") {
            m_transEngine = TransEngine::DeepseekJson;
        }
        else if (transEngineStr == "Sakura") {
            m_transEngine = TransEngine::Sakura;
        }
        else if (transEngineStr == "DumpName") {
            m_transEngine = TransEngine::DumpName;
        }
        else if (transEngineStr == "NameTrans") {
            m_transEngine = TransEngine::NameTrans;
        }
        else if (transEngineStr == "GenDict") {
            m_transEngine = TransEngine::GenDict;
        }
        else if (transEngineStr == "Rebuild") {
            m_transEngine = TransEngine::Rebuild;
        }
        else if (transEngineStr == "ShowNormal") {
            m_transEngine = TransEngine::ShowNormal;
        }
        else {
            throw std::runtime_error("Invalid trans engine: " + transEngineStr);
        }

        const auto pluginConfigData = toml::uparse(filePluginConfigPath / L"NormalJson.toml");
        m_outputWithSrc = parseToml<bool>(configData, pluginConfigData, "plugins.NormalJson.output_with_src");

        m_batchSize = toml::find_or(configData, "common", "numPerRequestTranslate", 14);
        m_threadsNum = toml::find_or(configData, "common", "threadsNum", 1);
        m_sortMethod = toml::find_or(configData, "common", "sortMethod", "name");
        m_targetLang = toml::find_or(configData, "common", "targetLang", "zh-cn");
        m_splitFile = toml::find_or(configData, "common", "splitFile", "no");
        m_splitFileNum = toml::find_or(configData, "common", "splitFileNum", 10);
        m_cacheSearchDistance = toml::find_or(configData, "common", "cacheSearchDistance", 5);
        m_saveCacheInterval = toml::find_or(configData, "common", "saveCacheInterval", 1);
        m_linebreakSymbol = toml::find_or(configData, "common", "linebreakSymbol", "auto");
        m_maxRetries = toml::find_or(configData, "common", "maxRetries", 5);
        m_contextHistorySize = toml::find_or(configData, "common", "contextHistorySize", 10);
        m_smartRetry = toml::find_or(configData, "common", "smartRetry", false);
        m_checkQuota = toml::find_or(configData, "common", "checkQuota", true);
        m_retransAllWhenFail = toml::find_or(configData, "common", "retransAllWhenFail", false);
        m_agentEnabled = toml::find_or(configData, "agent", "enabled", false);
        m_agentChunkSize = toml::find_or(configData, "agent", "chunkSize", 24);
        m_agentMaxTurnsPerChunk = toml::find_or(configData, "agent", "maxTurnsPerChunk", 6);
        m_agentSoftContextChars = toml::find_or(configData, "agent", "softContextChars", 48000);
        m_agentHardContextChars = toml::find_or(configData, "agent", "hardContextChars", 64000);
        m_agentLookaheadLines = toml::find_or(configData, "agent", "lookaheadLines", 80);
        m_agentSearchResultLimit = toml::find_or(configData, "agent", "searchResultLimit", 40);
        m_agentAllowCrossFileSearch = toml::find_or(configData, "agent", "allowCrossFileSearch", true);
        m_agentNativeFunctionCalling = toml::find_or(configData, "agent", "nativeFunctionCalling", "auto");
        m_agentFinalReconcileSingleThread = toml::find_or(configData, "agent", "finalReconcileSingleThread", true);
        m_agentRewriteMode = toml::find_or(configData, "agent", "rewriteMode", "queue_retranslate");

        if (m_agentEnabled) {
            if (m_transEngine != TransEngine::ForGalTsv) {
                throw std::invalid_argument("Agent 模式当前仅支持 ForGalTsv");
            }
            if (m_agentChunkSize <= 0 || m_agentMaxTurnsPerChunk <= 0 || m_agentSoftContextChars <= 0 || m_agentHardContextChars <= 0) {
                throw std::invalid_argument("Agent 模式配置无效");
            }
            if (m_agentSoftContextChars > m_agentHardContextChars) {
                std::swap(m_agentSoftContextChars, m_agentHardContextChars);
            }
            if (m_agentNativeFunctionCalling != "auto" && m_agentNativeFunctionCalling != "on" && m_agentNativeFunctionCalling != "off") {
                throw std::invalid_argument("agent.nativeFunctionCalling 必须是 auto/on/off");
            }
        }

        m_usePreDictInName = toml::find_or(configData, "dictionary", "usePreDictInName", false);
        m_usePostDictInName = toml::find_or(configData, "dictionary", "usePostDictInName", false);
        m_usePreDictInMsg = toml::find_or(configData, "dictionary", "usePreDictInMsg", true);
        m_usePostDictInMsg = toml::find_or(configData, "dictionary", "usePostDictInMsg", true);
        m_useGptDictToReplaceName = toml::find_or(configData, "dictionary", "useGPTDictInName", false);
        const std::string defaultDictFolder = toml::find_or(configData, "dictionary", "defaultDictFolder", "BaseConfig/Dict");
        const fs::path defaultDictFolderPath = ascii2Wide(defaultDictFolder);

        auto loadDictsFunc = [&]<typename DictionaryType>(const std::string& dictType, DictionaryType& dict)
            {
                const auto dictFileNames = toml::find<
                    std::optional<std::vector<std::string>>
                >(configData, "dictionary", dictType + "Dict");
                if (!dictFileNames) {
                    return;
                }
                for (const auto& dictFileName : *dictFileNames) {
                    fs::path dictPath = m_projectDir / ascii2Wide(dictFileName);
                    if (fs::exists(dictPath)) {
                        dict->loadFromFile(dictPath);
                    }
                    else {
                        dictPath = defaultDictFolderPath / ascii2Wide(dictType) / ascii2Wide(dictFileName);
                        if (fs::exists(dictPath)) {
                            dict->loadFromFile(dictPath);
                        }
                    }
                }
                dict->sort();
            };
        m_preDictionary = std::make_unique<NormalDictionary>(m_projectDir, m_luaManager, m_pythonManager, m_logger);
        loadDictsFunc("pre", m_preDictionary);

        if (m_transEngine == TransEngine::DumpName) {
            return;
        }

        // 需要 API 和提示词
        if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {

            m_apiStrategy = toml::find_or(configData, "backendSpecific", "OpenAI-Compatible", "apiStrategy", "random");
            if (m_apiStrategy != "random" && m_apiStrategy != "fallback") {
                throw std::invalid_argument("apiStrategy must be random or fallback");
            }
            int apiTimeOutSecond = toml::find_or(configData, "backendSpecific", "OpenAI-Compatible", "apiTimeout", 120);
            m_apiTimeOutMs = apiTimeOutSecond * 1000;

            const auto apisArr = toml::find<
                std::vector<toml::table>
            >(configData, "backendSpecific", "OpenAI-Compatible", "apis");

            std::vector<TranslationApi> apis;
            for (const auto& apiTbl : apisArr) {
                if (apiTbl.contains("enable") && !apiTbl.at("enable").as_boolean()) {
                    continue;
                }
                TranslationApi api;
                if (apiTbl.contains("apikey") && !apiTbl.at("apikey").as_string().empty()) {
                    api.apikey = apiTbl.at("apikey").as_string();
                }
                else if (m_transEngine == TransEngine::Sakura) {
                    api.apikey = "sk-sakura";
                }
                if (apiTbl.contains("apiurl") && !apiTbl.at("apiurl").as_string().empty()) {
                    api.apiurl = cvt2StdApiUrl(apiTbl.at("apiurl").as_string());
                }
                else {
                    continue;
                }
                if (apiTbl.contains("modelName") && !apiTbl.at("modelName").as_string().empty()) {
                    api.modelName = apiTbl.at("modelName").as_string();
                }
                else if (m_transEngine == TransEngine::Sakura) {
                    api.modelName = "sakura";
                }
                else {
                    continue;
                }
                api.stream = apiTbl.contains("stream") && apiTbl.at("stream").as_boolean();
                if (apiTbl.contains("temperature")) {
                    api.temperature = apiTbl.at("temperature").as_floating();
                }
                if (apiTbl.contains("topP")) {
                    api.topP = apiTbl.at("topP").as_floating();
                }
                if (apiTbl.contains("frequencyPenalty")) {
                    api.frequencyPenalty = apiTbl.at("frequencyPenalty").as_floating();
                }
                if (apiTbl.contains("presencePenalty")) {
                    api.presencePenalty = apiTbl.at("presencePenalty").as_floating();
                }
                if (apiTbl.contains("extraKeys")) {
                    const toml::array& extraKeysArr = apiTbl.at("extraKeys").as_array();
                    for (const auto& extraKey : extraKeysArr) {
                        TranslationApi extraApi = api;
                        extraApi.apikey = extraKey.as_string();
                        if (extraApi.apikey.empty()) {
                            continue;
                        }
                        apis.push_back(std::move(extraApi));
                    }
                }
                if (api.apikey.empty()) {
                    continue;
                }
                apis.push_back(std::move(api));
            }
            if (apis.empty()) {
                throw std::invalid_argument("找不到可用的 apikey ");
            }
            else {
                m_apiPool = std::make_unique<APIPool>(m_logger);
                m_apiPool->loadApis(std::move(apis));
            }

            const fs::path projectPromptPath = m_projectDir / L"Prompt.toml";
            const bool hasProjectPrompt = fs::exists(projectPromptPath);
            const bool hasDefaultPrompt = fs::exists(defaultPromptPath);
            if (!hasProjectPrompt && !hasDefaultPrompt) {
                throw std::runtime_error("找不到 Prompt.toml 文件");
            }

            const auto projectPromptData = hasProjectPrompt ? toml::uparse(projectPromptPath) : toml::value{};
            const auto defaultPromptData = hasDefaultPrompt ? toml::uparse(defaultPromptPath) : toml::value{};

            const auto readPromptString = [&](const std::string& key) -> std::string
                {
                    if (hasProjectPrompt && projectPromptData.contains(key) && projectPromptData.at(key).is_string()) {
                        return projectPromptData.at(key).as_string();
                    }
                    if (hasDefaultPrompt && defaultPromptData.contains(key) && defaultPromptData.at(key).is_string()) {
                        return defaultPromptData.at(key).as_string();
                    }
                    throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", key));
                };

            std::string systemKey;
            std::string userKey;

            switch (m_transEngine)
            {
            case TransEngine::ForGalJson:
                systemKey = "FORGALJSON_SYSTEM";
                userKey = "FORGALJSON_TRANS_PROMPT_EN";
                break;
            case TransEngine::ForGalTsv:
                systemKey = "FORGALTSV_SYSTEM";
                userKey = "FORGALTSV_TRANS_PROMPT_EN";
                break;
            case TransEngine::ForNovelTsv:
                systemKey = "FORNOVELTSV_SYSTEM";
                userKey = "FORNOVELTSV_TRANS_PROMPT_EN";
                break;
            case TransEngine::DeepseekJson:
                systemKey = "DEEPSEEKJSON_SYSTEM_PROMPT";
                userKey = "DEEPSEEKJSON_TRANS_PROMPT";
                break;
            case TransEngine::Sakura:
                systemKey = "SAKURA_SYSTEM_PROMPT";
                userKey = "SAKURA_TRANS_PROMPT";
                break;
            case TransEngine::GenDict:
                systemKey = "GENDIC_SYSTEM";
                userKey = "GENDIC_PROMPT";
                break;
            case TransEngine::NameTrans:
                systemKey = "NAMETRANS_SYSTEM";
                userKey = "NAMETRANS_PROMPT";
                break;
            default:
                throw std::invalid_argument("未知的 TransEngine");
            }

            m_systemPrompt = readPromptString(systemKey);
            m_userPrompt = readPromptString(userKey);

            if (m_agentEnabled && m_transEngine == TransEngine::ForGalTsv) {
                m_agentSystemPrompt = readPromptString("FORGALTSV_AGENT_SYSTEM");
                m_agentUserPrompt = readPromptString("FORGALTSV_AGENT_PROMPT_EN");
                m_logger->info("Agent 模式提示词已加载，将使用 Prompt.toml 中的 FORGALTSV_AGENT_SYSTEM / FORGALTSV_AGENT_PROMPT_EN。");
            }
        }


        if (m_transEngine != TransEngine::NameTrans) {
            // 需要翻译预处理
            const std::string tokenizerBackend = toml::find_or(configData, "common", "tokenizerBackend", "MeCab");
            if (tokenizerBackend == "MeCab") {
                const std::string mecabDictDir = toml::find_or(configData, "common", "mecabDictDir", "BaseConfig/mecabDict/mecab-ipadic-utf8");
                m_logger->info("已配置 MeCab 分词器，首次使用时加载。");
                m_tokenizeSourceLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
            }
            else if (tokenizerBackend == "spaCy") {
                const std::string spaCyModelName = toml::find_or(configData, "common", "spaCyModelName", "ja_core_news_lg");
                m_logger->info("已配置 spaCy 分词器，首次使用时加载。");
                m_tokenizeSourceLangFunc = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger);
            }
            else if (tokenizerBackend == "Stanza") {
                const std::string stanzaLang = toml::find_or(configData, "common", "stanzaLang", "ja");
                m_logger->info("已配置 Stanza 分词器，首次使用时加载。");
                m_tokenizeSourceLangFunc = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger);
            }
            else {
                throw std::invalid_argument(std::format("无效的 tokenizerBackend: {}", tokenizerBackend));
            }

            const auto textPlugins = toml::find<
                std::optional<std::vector<std::string>>
            >(configData, "plugins", "textPlugins");
            if (textPlugins) {
                registerPlugins(m_textPlugins, *textPlugins, m_projectDir, m_otherCacheDir, m_pythonManager, m_luaManager, m_logger, configData, true);
            }
        }
        else {
            m_gptDictionary = std::make_unique<GptDictionary>(m_projectDir, m_otherCacheDir, m_tokenizeSourceLangFunc,
                m_luaManager, m_pythonManager, m_logger);
            loadDictsFunc("gpt", m_gptDictionary);
        }


        // 需要翻译中/后处理
        if (m_transEngine != TransEngine::ShowNormal && m_transEngine != TransEngine::GenDict && m_transEngine != TransEngine::NameTrans) {

            m_gptDictionary = std::make_unique<GptDictionary>(m_projectDir, m_otherCacheDir, m_tokenizeSourceLangFunc,
                m_luaManager, m_pythonManager, m_logger);
            loadDictsFunc("gpt", m_gptDictionary);
            m_postDictionary = std::make_unique<NormalDictionary>(m_projectDir, m_luaManager, m_pythonManager, m_logger);
            loadDictsFunc("post", m_postDictionary);

            const auto textPlugins = toml::find<
                std::optional<std::vector<std::string>>
            >(configData, "plugins", "textPlugins");
            if (textPlugins) {
                registerPlugins(m_textPlugins, *textPlugins, m_projectDir, m_otherCacheDir, m_pythonManager, m_luaManager, m_logger, configData, false);
            }

            m_problemAnalyzer = std::make_unique<ProblemAnalyzer>(m_gptDictionary, m_targetLang, m_logger);
            const auto problemList = toml::find<
                std::optional<std::vector<std::string>>
            >(configData, "problemAnalyze", "problemList");
            if (problemList) {
                const std::string punctSet = toml::find_or(configData, "problemAnalyze", "punctSet", "（()）：:*[]{}<>『』「」“”;；'/\\");
                const std::string codePage = toml::find_or(configData, "problemAnalyze", "codePage", "gbk");
                double langProbability = toml::find_or(configData, "problemAnalyze", "langProbability", 0.94);
                m_problemAnalyzer->loadProblems(*problemList, punctSet, codePage, langProbability);
            }

            const auto retranslKeys = 
                toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "retranslKeys");
            if (retranslKeys) {
                for (const auto& elem : *retranslKeys) {
                    if (elem.is_string()) {
                        GppConditionPattern pattern;
                        pattern.conditionTarget = CachePart::Problems;
                        pattern.conditionReg.setPattern(elem.as_string()).setModifier(defaultRegCompileModifier).compile();
                        if (!pattern.conditionReg) {
                            throw std::runtime_error(std::format("retranslKeys 正则表达式 [{}] 编译失败", elem.as_string()));
                        }
                        GPPCondition cond{ std::move(pattern) };
                        CheckSeCondFunc checkFunc = [condr = std::move(cond)](const Sentence* se) -> bool
                            {
                                return checkGppCondition(condr, se);
                            };
                        m_retranslKeys.push_back(std::move(checkFunc));
                    }
                    else if (elem.is_array() || elem.is_table()) {
                        CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, m_projectDir, m_pythonManager, m_luaManager, m_logger);
                        m_retranslKeys.push_back(std::move(checkFunc));
                    }
                    else {
                        throw std::invalid_argument("retranslKeys 的元素必须是字符串、表或表数组");
                    }
                }
            }

            const auto skipProblems = 
                toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "skipProblems");
            if (skipProblems) {
                for (const auto& elem : *skipProblems) {
                    if (elem.is_string()) {
                        m_skipProblems.push_back({ jpc::Regex(elem.as_string(), defaultRegCompileModifier), std::nullopt });
                    }
                    else if (elem.is_array() && elem.size() > 0) {
                        if (!elem[0].is_string()) {
                            throw std::invalid_argument("skipProblems 的内联表数组第一个元素必须是字符串");
                        }
                        jpc::Regex pattern(elem[0].as_string(), defaultRegCompileModifier);
                        if (elem.size() == 1) {
                            m_skipProblems.push_back({ std::move(pattern), std::nullopt });
                        }
                        else {
                            CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, m_projectDir, m_pythonManager, m_luaManager, m_logger);
                            m_skipProblems.push_back({ std::move(pattern), std::move(checkFunc) });
                        }
                    }
                    else {
                        throw std::invalid_argument("skipProblems 的元素必须是字符串或表数组");
                    }
                }
            }

            const auto overwriteCompareObj = 
                toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "overwriteCompareObj");
            if (overwriteCompareObj) {
                for (const auto& tbl : *overwriteCompareObj) {
                    const std::string problemKey = toml::find_or(tbl, "problemKey", "");
                    if (problemKey.empty()) {
                        continue;
                    }
                    std::string base = toml::find_or(tbl, "base", "");
                    if (base.empty()) {
                        base = "orig_text";
                    }
                    std::string check = toml::find_or(tbl, "check", "");
                    if (check.empty()) {
                        check = "trans_preview";
                    }
                    m_problemAnalyzer->overwriteCompareObj(problemKey, base, check);
                }
            }

        }
    }
    catch (const toml::exception& e) {
        m_logger->critical("项目配置文件解析失败");
        throw std::runtime_error(e.what());
    }
}

void NormalJsonTranslator::preProcess(Sentence* se) {

    // se->name = se->name;
    // se->names = se->names;
    // 相当于省略了 name_org 这一项，因为最原始人名并不在缓存里输出
    // se->name 实际上相当于 se->name_preproc
    se->pre_processed_text = se->original_text;

    for (auto& plugin : m_textPlugins) {
        plugin->dPreRun(se);
    }

    if (se->nameType != NameType::None && m_usePreDictInName) {
        if (se->nameType == NameType::Single) {
            se->name = m_preDictionary->doReplace(se, CachePart::Name);
        }
        else {
            for (auto& name : se->names) {
                se->name = std::move(name);
                name = m_preDictionary->doReplace(se, CachePart::Name);
            }
        }
    }

    if (m_linebreakSymbol == "auto") {
        if (se->pre_processed_text.contains("<br>")) {
            se->originalLinebreak = "<br>";
        }
        else if (se->pre_processed_text.contains("\\r\\n")) {
            se->originalLinebreak = "\\r\\n";
        }
        else if (se->pre_processed_text.contains("\\n:")) {
            se->originalLinebreak = "\\n:";
        }
        else if (se->pre_processed_text.contains("\\n")) {
            se->originalLinebreak = "\\n";
        }
        else if (se->pre_processed_text.contains("\\r")) {
            se->originalLinebreak = "\\r";
        }
        else if (se->pre_processed_text.contains("\r\n")) {
            se->originalLinebreak = "\r\n";
        }
        else if (se->pre_processed_text.contains("\n")) {
            se->originalLinebreak = "\n";
        }
        else if (se->pre_processed_text.contains("\r")) {
            se->originalLinebreak = "\r";
        }
        else if (se->pre_processed_text.contains("[r][n]")) {
            se->originalLinebreak = "[r][n]";
        }
        else if (se->pre_processed_text.contains("[n]")) {
            se->originalLinebreak = "[n]";
        }
        else if (se->pre_processed_text.contains("[r]")) {
            se->originalLinebreak = "[r]";
        }
    }
    else {
        se->originalLinebreak = m_linebreakSymbol;
    }
    if (!se->originalLinebreak.empty()) {
        replaceStrInplace(se->pre_processed_text, se->originalLinebreak, "<br>");
    }
    replaceStrInplace(se->pre_processed_text, "\t", "<tab>");
    if (m_usePreDictInMsg) {
        se->pre_processed_text = m_preDictionary->doReplace(se, CachePart::PreprocText);
    }

    for (auto& plugin : m_textPlugins) {
        plugin->preRun(se);
    }

}

void NormalJsonTranslator::postProcess(Sentence* se) {

    se->name_preview = se->name;
    se->names_preview = se->names;
    se->translated_preview = se->pre_translated_text;
    se->problems.clear();

    if (se->translated_preview.starts_with("(Failed to translate)")) {
        se->problems.push_back("翻译失败");
        se->notAnalyzeProblem = true;
    }
    if (se->translated_preview.starts_with("(GPPCProblem:")) {
        if (const size_t pos = se->translated_preview.find(')'); pos != std::string::npos) {
            se->problems.push_back(std::format("GPPCProblem:{}", std::string_view(se->translated_preview.data() + 13, pos - 13)));
            se->translated_preview = se->translated_preview.substr(pos + 1);
        }
    }
    else if (se->translated_preview.contains("GPPCProblem")) {
        se->problems.push_back("错误的 GPPCProblem 格式");
    }

    for (auto& plugin : m_textPlugins) {
        plugin->postRun(se);
    }

    if (m_usePostDictInMsg) {
        se->translated_preview = m_postDictionary->doReplace(se, CachePart::TransPreview);
    }
    replaceStrInplace(se->translated_preview, "<tab>", "\t");
    if (!se->originalLinebreak.empty()) {
        replaceStrInplace(se->translated_preview, "<br>", se->originalLinebreak);
    }

    auto replaceName = [&]()
        {
            if (m_useGptDictToReplaceName) {
                se->name_preview = m_gptDictionary->doReplace(se, CachePart::NamePreview);
            }
            if (!se->name_preview.empty()) {
                const auto it = m_nameMap.find(se->name_preview);
                if (it != m_nameMap.end() && !it->second.empty()) {
                    se->name_preview = it->second;
                }
            }
            if (m_usePostDictInName) {
                se->name_preview = m_postDictionary->doReplace(se, CachePart::NamePreview);
            }
        };

    if (se->nameType != NameType::None) {
        if (se->nameType == NameType::Single) {
            replaceName();
        }
        else {
            for (auto& name_preivew : se->names_preview) {
                se->name_preview = std::move(name_preivew);
                replaceName();
                name_preivew = std::move(se->name_preview);
            }
        }
    }

    for (auto& plugin : m_textPlugins) {
        plugin->dPostRun(se);
    }

    if (!se->notAnalyzeProblem) {
        m_problemAnalyzer->analyze(se);
    }

    std::erase_if(se->problems, [&](std::string& problem)
        {
            return std::ranges::any_of(m_skipProblems, [&](const SkipProblemCondition& skipProblemCondition)
                {
                    const bool problemMatch = checkString(skipProblemCondition.first, problem);
                    if (!problemMatch) {
                        return false;
                    }
                    if (!skipProblemCondition.second.has_value()) {
                        return true;
                    }
                    else {
                        return [&]()
	                        {
                                problem = "Current problem:" + problem;
                                const bool result = skipProblemCondition.second.value()(se);
                                problem = problem.substr(16);
                                return result;
                            }();
                    }
                });
        });
}


bool NormalJsonTranslator::translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId) {

    for (Sentence* se : batch) {
        if (se->pre_processed_text.empty()) {
            se->complete = true;
            ++m_completedSentences;
            m_controller->updateBar(); // 为空不翻译
        }
    }

    int retryCount = 0;
    std::string contextHistory = buildContextHistory(batch, m_transEngine, m_contextHistorySize, 1024);
    std::string glossary = m_gptDictionary->generatePrompt(batch, m_transEngine);

    while (retryCount == 0 || retryCount < m_maxRetries) {

        if (m_controller->shouldStop()) {
            return false;
        }

        std::vector<Sentence*> batchToTransThisRound = batch | std::views::filter([](const Sentence* se) { return !se->complete; }) 
    	        | std::ranges::to<std::vector>();

        if (batchToTransThisRound.empty()) {
            return true;
        }

        if (m_smartRetry && retryCount == 2 && batchToTransThisRound.size() > 1) {
            m_logger->warn("[线程 {}] [文件 {}] 开始拆分批次进行重试...", threadId, wide2Ascii(relInputPath));

            const size_t mid = batchToTransThisRound.size() / 2;
            std::span<Sentence*> batchToTransThisRoundSpan(batchToTransThisRound);
            std::span<Sentence*> firstHalf = batchToTransThisRoundSpan.subspan(0, mid);
            std::span<Sentence*> secondHalf = batchToTransThisRoundSpan.subspan(mid);

            bool firstOk = translateBatch(relInputPath, firstHalf, backgroundText, threadId);
            bool secondOk = translateBatch(relInputPath, secondHalf, backgroundText, threadId);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] [文件 {}] 清空上下文后再次尝试...", threadId, wide2Ascii(relInputPath));
            contextHistory.clear();
            backgroundText.clear();
        }


        const std::string inputProblems = std::ranges::fold_left(batchToTransThisRound
            | std::views::transform([](const Sentence* se) { return se->problems; })
            | std::views::join, std::string{}, [](const auto& acc, const auto& value)
            {
                if (!acc.contains(value)) {
                    return acc + value + "\n";
                }
                return acc;
            });

        std::string inputBlock;
        absl::btree_map<int, Sentence*> id2SentenceMap; // 用于 TSV/JSON 
        fillBlockAndMap(batchToTransThisRound, id2SentenceMap, inputBlock, m_transEngine);

        std::string logBlock;
        if (!inputProblems.empty()) {
            logBlock += "\nProblems:\n" + inputProblems;
        }
        if (m_logger->should_log(spdlog::level::debug) && !backgroundText.empty()) {
            logBlock += "\nBackground:\n" + backgroundText + "\n";
        }
        if (m_logger->should_log(spdlog::level::trace) && !contextHistory.empty()) {
            logBlock += "\nContext:\n" + contextHistory + "\n";
        }
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + glossary;
        }
        logBlock += "\ninputBlock:\n" + inputBlock;
        m_logger->info("[线程 {}] [文件 {}] 开始翻译:\n{}", threadId, wide2Ascii(relInputPath), logBlock);
        std::string promptReq = m_userPrompt;
        replaceStrInplace(promptReq, "[Problem Description]", inputProblems.empty() ? "None" : inputProblems);
        replaceStrInplace(promptReq, "[Background Description]", backgroundText.empty() ? "None" : backgroundText);
        replaceStrInplace(promptReq, "[Input]", inputBlock);
        replaceStrInplace(promptReq, "[TargetLang]", m_targetLang);
        replaceStrInplace(promptReq, "[Glossary]", glossary.empty() ? "None" : glossary);

        json messages = json::array({ {{"role", "system"}, {"content", m_systemPrompt}} });
        if (!contextHistory.empty()) {
            messages.push_back({ {"role", "user"}, {"content", "<input>(...truncated history source texts...)</input><output>\n"} });
            messages.push_back({ {"role", "assistant"}, {"content", contextHistory} });
        }
        messages.push_back({ {"role", "user"}, {"content", promptReq} });

        const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt.has_value()) {
            throw std::runtime_error("没有可用的API Key了");
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);

        /*bool checkResponse(const ApiResponse& response, const std::unique_ptr<APIPool>& m_apiPool, const TranslationApi& currentAPI,
            const std::filesystem::path& relInputPath, const std::string& m_apiStrategy, const std::shared_ptr<spdlog::logger>& m_logger,
            int& retryCount, int threadId, bool m_checkQuota);*/
        if (!checkResponse(
            response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }
        else {
            m_logger->trace("[线程 {}] [文件 {}] 成功响应，响应内容:\n{}", threadId, wide2Ascii(relInputPath), response.content);
        }

        // --- 如果请求成功，则继续解析 ---
        //int parseContent(std::string& content, std::span<Sentence*> batchToTransThisRound, absl::btree_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
        //    const std::shared_ptr<IController>& controller, std::string& backgroudText, std::atomic<int>& completedSentences,
        //    TransEngine transEngine, bool showBackgroundText, bool retransAllWhenFail);
        int parsedCount = parseContent(response.content, batchToTransThisRound, id2SentenceMap, currentApi.modelName,
            m_controller, backgroundText, m_completedSentences,
            m_transEngine, m_logger->should_log(spdlog::level::debug), m_retransAllWhenFail);

        if (parsedCount != batchToTransThisRound.size()) {
            ++retryCount;
            if (!m_controller->shouldStop()) {
                m_logger->warn("[线程 {}] [文件 {}] 解析失败或不完整 ({} / {}), 进行第 {} 次重试..., 解析结果: \n{}", 
                    threadId, wide2Ascii(relInputPath), parsedCount, batchToTransThisRound.size(), retryCount, response.content);
            }
            continue;
        }
        else {
            m_logger->info("[线程 {}] [文件 {}] 成功解析 {} 句，解析结果: \n{}", threadId, wide2Ascii(relInputPath), parsedCount, response.content);
        }

        return true;
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](const Sentence* s) { return !s->complete; })) {
        ++failedCount;
        se->pre_translated_text = "(Failed to translate)" + se->pre_processed_text;
        se->complete = true;
        ++m_completedSentences;
        m_controller->updateBar(); // 失败
    }
    m_logger->error("[线程 {}] [文件 {}] 批次翻译在 {} 次重试后彻底失败，共翻译 {} / {} 句。",
        threadId, wide2Ascii(relInputPath), retryCount, batch.size() - failedCount, batch.size());
    return false;
}

#if 0
bool NormalJsonTranslator::translateBatchAgent(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId) {
    for (Sentence* se : batch) {
        if (se->pre_processed_text.empty()) {
            se->complete = true;
            ++m_completedSentences;
            m_controller->updateBar();
        }
    }

    auto currentChunk = [&]() {
        return batch
            | std::views::filter([](const Sentence* se) { return !se->complete; })
            | std::ranges::to<std::vector>();
    };

    auto resolveInputPath = [&](const fs::path& relPath) {
        if (m_needsCombining && fs::exists(m_inputCacheDir / relPath)) {
            return m_inputCacheDir / relPath;
        }
        return m_inputDir / relPath;
    };

    auto loadFileNote = [&](const fs::path& targetRelPath) {
        std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
        return loadJsonFileOrDefault(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), json::object());
    };

    auto saveFileNote = [&](const fs::path& targetRelPath, const json& note) {
        std::lock_guard<std::mutex> lock(m_agentFileNotesMutex);
        saveJsonFilePretty(buildAgentFileNotePath(m_agentFileNotesDir, targetRelPath), note);
    };

    auto loadTermLedger = [&]() {
        std::lock_guard<std::mutex> lock(m_agentStateMutex);
        return loadJsonFileOrDefault(m_agentTermLedgerPath, json::object());
    };

    auto saveTermLedger = [&](const json& ledger) {
        std::lock_guard<std::mutex> lock(m_agentStateMutex);
        saveJsonFilePretty(m_agentTermLedgerPath, ledger);
    };

    auto loadRewriteQueue = [&]() {
        std::lock_guard<std::mutex> lock(m_agentStateMutex);
        return loadJsonFileOrDefault(m_agentRewriteQueuePath, json::array());
    };

    auto saveRewriteQueue = [&](const json& queue) {
        std::lock_guard<std::mutex> lock(m_agentStateMutex);
        saveJsonFilePretty(m_agentRewriteQueuePath, queue);
    };

    auto updateRunState = [&](const std::string& status, int lastCommittedIndex = -1, const std::string& leaseOwner = {}) {
        std::lock_guard<std::mutex> lock(m_agentStateMutex);
        json state = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
        if (!state.contains("files") || !state["files"].is_array()) {
            state["files"] = json::array();
        }
        const std::string relPathStr = wide2Ascii(relInputPath);
        auto it = std::ranges::find_if(state["files"], [&](const json& item) {
            return item.value("file", "") == relPathStr;
        });
        if (it == state["files"].end()) {
            state["files"].push_back({
                {"file", relPathStr},
                {"status", status},
                {"lease_owner", leaseOwner},
                {"last_committed_index", lastCommittedIndex},
                {"updated_at", nowTimestampString()}
            });
        }
        else {
            (*it)["status"] = status;
            (*it)["lease_owner"] = leaseOwner;
            if (lastCommittedIndex >= 0) {
                (*it)["last_committed_index"] = lastCommittedIndex;
            }
            (*it)["updated_at"] = nowTimestampString();
        }
        state["updated_at"] = nowTimestampString();
        saveJsonFilePretty(m_agentRunStatePath, state);
    };

    auto loadCacheDstMap = [&](const fs::path& targetRelPath) {
        absl::flat_hash_map<int, json> cacheMap;
        const fs::path cachePath = m_transCacheDir / targetRelPath;
        if (!fs::exists(cachePath)) {
            return cacheMap;
        }
        try {
            std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
            std::ifstream ifs(cachePath, std::ios::binary);
            json cacheJson = json::parse(ifs);
            for (const auto& item : cacheJson) {
                const int index = item.value("index", -1);
                if (index >= 0) {
                    cacheMap[index] = item;
                }
            }
        }
        catch (...) {}
        return cacheMap;
    };

    auto readLinesTool = [&](const json& arguments) {
        const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
        const int start = std::max(0, arguments.value("start", 0));
        const int count = std::max(0, arguments.value("count", m_agentLookaheadLines));
        const bool includeSrc = arguments.value("include_src", true);
        const bool includeDst = arguments.value("include_dst", true);

        json result = {
            {"file", wide2Ascii(targetRelPath)},
            {"lines", json::array()}
        };

        try {
            std::ifstream ifs(resolveInputPath(targetRelPath), std::ios::binary);
            ordered_json inputJson = ordered_json::parse(ifs);
            const auto cacheMap = loadCacheDstMap(targetRelPath);
            for (int i = start; i < (int)inputJson.size() && i < start + count; ++i) {
                json line = { {"id", i} };
                if (inputJson[i].contains("name")) {
                    line["name"] = inputJson[i]["name"];
                }
                if (inputJson[i].contains("names")) {
                    line["names"] = inputJson[i]["names"];
                }
                if (includeSrc) {
                    line["src"] = inputJson[i].value("message", "");
                }
                if (includeDst) {
                    if (const auto it = cacheMap.find(i); it != cacheMap.end()) {
                        line["dst"] = it->second.value("translated_preview", it->second.value("pre_translated_text", ""));
                    }
                }
                result["lines"].push_back(std::move(line));
            }
        }
        catch (const std::exception& e) {
            result["error"] = e.what();
        }
        return result;
    };

    auto listFilesTool = [&](const json& arguments) {
        const std::string pattern = str2Lower(arguments.value("pattern", ""));
        const int limit = sanitizeToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
        json files = json::array();
        for (const auto& relFile : m_agentKnownRelFiles) {
            const std::string relFileStr = wide2Ascii(relFile);
            if (!pattern.empty() && !str2Lower(relFileStr).contains(pattern)) {
                continue;
            }
            files.push_back(relFileStr);
            if ((int)files.size() >= limit) {
                break;
            }
        }
        return json{ {"files", files} };
    };

    auto searchTextTool = [&](const json& arguments) {
        const std::string query = arguments.value("query", "");
        const std::string queryLower = str2Lower(query);
        const std::string scope = arguments.value("scope", "current_file");
        const int limit = sanitizeToolLimit(arguments.value("limit", m_agentSearchResultLimit), m_agentSearchResultLimit);
        std::vector<fs::path> targetFiles;

        if (scope == "specified_file") {
            targetFiles.push_back(ascii2Wide(arguments.value("file", wide2Ascii(relInputPath))));
        }
        else if (scope == "all_files" && m_agentAllowCrossFileSearch) {
            targetFiles = m_agentKnownRelFiles;
        }
        else {
            targetFiles.push_back(relInputPath);
        }

        json matches = json::array();
        auto pushMatch = [&](json&& item) {
            if ((int)matches.size() < limit) {
                matches.push_back(std::move(item));
            }
        };

        const json termLedger = loadTermLedger();
        for (const auto& item : termLedger.items()) {
            if ((int)matches.size() >= limit) {
                break;
            }
            const std::string term = item.key();
            const json& entry = item.value();
            const std::string targetTerm = entry.value("target_term", "");
            if (str2Lower(term).contains(queryLower) || str2Lower(targetTerm).contains(queryLower)) {
                pushMatch(json{
                    {"type", "term"},
                    {"term", term},
                    {"target_term", targetTerm},
                    {"status", entry.value("status", "tentative")},
                    {"note", entry.value("note", "")}
                });
            }
        }

        for (const auto& targetRelPath : targetFiles) {
            if ((int)matches.size() >= limit) {
                break;
            }
            try {
                std::ifstream ifs(resolveInputPath(targetRelPath), std::ios::binary);
                ordered_json inputJson = ordered_json::parse(ifs);
                const auto cacheMap = loadCacheDstMap(targetRelPath);
                for (const auto& [index, item] : inputJson | std::views::enumerate) {
                    if ((int)matches.size() >= limit) {
                        break;
                    }
                    const std::string src = item.value("message", "");
                    std::string dst;
                    if (const auto it = cacheMap.find((int)index); it != cacheMap.end()) {
                        dst = it->second.value("translated_preview", it->second.value("pre_translated_text", ""));
                    }
                    if (!str2Lower(src).contains(queryLower) && !str2Lower(dst).contains(queryLower)) {
                        continue;
                    }
                    pushMatch(json{
                        {"type", "line"},
                        {"file", wide2Ascii(targetRelPath)},
                        {"id", (int)index},
                        {"src", src},
                        {"dst", dst}
                    });
                }

                if ((int)matches.size() >= limit) {
                    continue;
                }
                const json fileNote = loadFileNote(targetRelPath);
                if (!fileNote.empty() && str2Lower(fileNote.dump()).contains(queryLower)) {
                    pushMatch(json{
                        {"type", "file_note"},
                        {"file", wide2Ascii(targetRelPath)},
                        {"note", fileNote}
                    });
                }
            }
            catch (...) {}
        }

        return json{ {"matches", matches} };
    };

    auto getTermTool = [&](const json& arguments) {
        const std::string term = arguments.value("term", "");
        const json ledger = loadTermLedger();
        return json{
            {"term", term},
            {"entry", ledger.contains(term) ? ledger.at(term) : json(nullptr)}
        };
    };

    auto getFileNoteTool = [&](const json& arguments) {
        const fs::path targetRelPath = ascii2Wide(arguments.value("file", wide2Ascii(relInputPath)));
        return json{
            {"file", wide2Ascii(targetRelPath)},
            {"note", loadFileNote(targetRelPath)}
        };
    };

    auto executeToolCalls = [&](const std::vector<AgentToolCallRequest>& calls) {
        json toolResults = json::array();
        for (const auto& call : calls) {
            json result = {
                {"id", call.id},
                {"name", call.name}
            };
            try {
                if (call.name == "list_files") {
                    result["result"] = listFilesTool(call.arguments);
                }
                else if (call.name == "read_lines") {
                    result["result"] = readLinesTool(call.arguments);
                }
                else if (call.name == "search_text") {
                    result["result"] = searchTextTool(call.arguments);
                }
                else if (call.name == "get_term") {
                    result["result"] = getTermTool(call.arguments);
                }
                else if (call.name == "get_file_note") {
                    result["result"] = getFileNoteTool(call.arguments);
                }
                else {
                    result["error"] = std::format("未知工具: {}", call.name);
                }
            }
            catch (const std::exception& e) {
                result["error"] = e.what();
            }
            toolResults.push_back(std::move(result));
        }
        return toolResults;
    };

    auto termLedgerExcerpt = [&]() {
        const json ledger = loadTermLedger();
        json excerpt = json::array();
        for (const auto& item : ledger.items()) {
            const std::string term = item.key();
            const json& entry = item.value();
            excerpt.push_back({
                {"term", term},
                {"target_term", entry.value("target_term", "")},
                {"status", entry.value("status", "tentative")},
                {"category", entry.value("category", "")}
            });
            if ((int)excerpt.size() >= m_agentSearchResultLimit) {
                break;
            }
        }
        return excerpt.dump(2);
    };

    auto buildBaseMessages = [&](const std::string& rollingSummary, const json& fileNote) {
        absl::btree_map<int, Sentence*> id2SentenceMap;
        std::string inputBlock;
        fillBlockAndMap(batch, id2SentenceMap, inputBlock, m_transEngine);
        std::string userPrompt = std::format(
            "You are working in GalTransl++ experimental agent mode.\n"
            "Current file: {0}\n"
            "Current chunk ids: {1}-{2}\n"
            "You may either call read-only tools, return a compact_context action, or return a commit action.\n"
            "Never write files directly. All writes must go through commit.\n"
            "Commit requirements:\n"
            "1. action must be commit.\n"
            "2. translations must cover every current chunk id exactly once.\n"
            "3. Each translation item must include id and dst.\n"
            "4. term_updates/rewrite_requests/file_note_patch are optional.\n"
            "5. Return valid JSON only.\n\n"
            "If context is near limit, prefer compact_context and summarize confirmed terms, tentative terms, unresolved questions, scene state, and cross-file clues.\n\n"
            "Rolling summary:\n{3}\n\n"
            "Current file note:\n{4}\n\n"
            "Known terms excerpt:\n{5}\n\n"
            "Current chunk TSV:\nNAME\\tSRC\\tID\n{6}",
            wide2Ascii(relInputPath),
            batch.front()->index,
            batch.back()->index,
            rollingSummary.empty() ? "None" : rollingSummary,
            fileNote.empty() ? "None" : fileNote.dump(2),
            termLedgerExcerpt(),
            inputBlock
        );

        return json::array({
            {{"role", "system"}, {"content", m_systemPrompt + "\nYou are now in a structured agent workflow. Output valid JSON only unless using native tool calls."}},
            {{"role", "user"}, {"content", userPrompt}}
        });
    };

    auto approximateMessagesChars = [](const json& messages) {
        size_t total = 0;
        for (const auto& item : messages) {
            total += item.dump().size();
        }
        return total;
    };

    auto mergeFileNotePatch = [&](json& note, const json& patch) {
        if (!patch.is_object()) {
            return;
        }
        for (const auto& item : patch.items()) {
            note[item.key()] = item.value();
        }
    };

    auto appendOccurrence = [](json& entry, const fs::path& file, int id) {
        if (!entry.contains("occurrences") || !entry["occurrences"].is_array()) {
            entry["occurrences"] = json::array();
        }
        const std::string fileStr = wide2Ascii(file);
        const bool exists = std::ranges::any_of(entry["occurrences"], [&](const json& occurrence) {
            return occurrence.value("file", "") == fileStr && occurrence.value("id", -1) == id;
        });
        if (!exists) {
            entry["occurrences"].push_back({ {"file", fileStr}, {"id", id} });
        }
    };

    auto enqueueRewriteRequest = [&](json& queue, const json& request) {
        if (!request.is_object()) {
            return;
        }
        const std::string file = request.value("file", "");
        const int id = request.value("id", -1);
        const std::string sourceTerm = request.value("source_term", "");
        const bool exists = std::ranges::any_of(queue, [&](const json& item) {
            return item.value("file", "") == file && item.value("id", -1) == id && item.value("source_term", "") == sourceTerm;
        });
        if (!exists) {
            queue.push_back(request);
        }
    };

    auto applyCommit = [&](const AgentProtocolResponse& protocol, const std::string& modelName, int& committedCount) {
        std::unordered_map<int, json> translationMap;
        for (const auto& item : protocol.translations) {
            if (!item.is_object()) {
                continue;
            }
            const int id = item.value("id", -1);
            const std::string dst = item.value("dst", "");
            if (id >= 0 && !dst.empty()) {
                translationMap.insert_or_assign(id, item);
            }
        }

        const std::vector<Sentence*> pending = currentChunk();
        if (translationMap.size() != pending.size()) {
            throw std::runtime_error("commit 未覆盖当前 chunk 的全部句子");
        }

        committedCount = 0;
        for (Sentence* se : pending) {
            const auto it = translationMap.find(se->index);
            if (it == translationMap.end()) {
                throw std::runtime_error(std::format("commit 缺少句子 {}", se->index));
            }
            se->pre_translated_text = it->second.value("dst", "");
            if (se->pre_translated_text.empty()) {
                throw std::runtime_error(std::format("commit 句子 {} 的 dst 为空", se->index));
            }
            se->translated_by = modelName;
            se->complete = true;
            ++committedCount;
        }

        if (committedCount > 0) {
            m_completedSentences += committedCount;
            m_controller->updateBar(committedCount);
        }

        json fileNote = loadFileNote(relInputPath);
        mergeFileNotePatch(fileNote, protocol.fileNotePatch);
        if (protocol.summary.contains("rolling_context") && protocol.summary["rolling_context"].is_string()) {
            backgroundText = protocol.summary["rolling_context"].get<std::string>();
            fileNote["rolling_context"] = backgroundText;
        }
        else if (protocol.summary.contains("context") && protocol.summary["context"].is_string()) {
            backgroundText = protocol.summary["context"].get<std::string>();
            fileNote["rolling_context"] = backgroundText;
        }
        saveFileNote(relInputPath, fileNote);

        json termLedger = loadTermLedger();
        json rewriteQueue = loadRewriteQueue();
        const std::vector<int> currentChunkIds = pending | std::views::transform([](const Sentence* se) { return se->index; }) | std::ranges::to<std::vector>();
        for (const auto& update : protocol.termUpdates) {
            if (!update.is_object()) {
                continue;
            }
            const std::string sourceTerm = update.value("source_term", update.value("term", ""));
            const std::string targetTerm = update.value("target_term", update.value("translation", ""));
            if (sourceTerm.empty() || targetTerm.empty()) {
                continue;
            }
            json& entry = termLedger[sourceTerm];
            const std::string oldTarget = entry.value("target_term", "");
            entry["target_term"] = targetTerm;
            entry["status"] = update.value("status", entry.value("status", "tentative"));
            entry["category"] = update.value("category", entry.value("category", ""));
            entry["note"] = update.value("note", entry.value("note", ""));
            if (update.contains("line_ids") && update["line_ids"].is_array()) {
                for (const auto& idVal : update["line_ids"]) {
                    appendOccurrence(entry, relInputPath, idVal.get<int>());
                }
            }
            else {
                for (const int id : currentChunkIds) {
                    appendOccurrence(entry, relInputPath, id);
                }
            }

            if (!m_agentReconciling && !oldTarget.empty() && oldTarget != targetTerm && m_agentRewriteMode == "queue_retranslate") {
                for (const auto& occurrence : entry["occurrences"]) {
                    enqueueRewriteRequest(rewriteQueue, {
                        {"file", occurrence.value("file", "")},
                        {"id", occurrence.value("id", -1)},
                        {"source_term", sourceTerm},
                        {"old_target", oldTarget},
                        {"new_target", targetTerm}
                    });
                }
            }
        }

        if (!m_agentReconciling) {
            for (const auto& request : protocol.rewriteRequests) {
                enqueueRewriteRequest(rewriteQueue, request);
            }
        }

        saveTermLedger(termLedger);
        saveRewriteQueue(rewriteQueue);
        updateRunState("in_progress", pending.back()->index, std::format("thread-{}", threadId));
    };

    int retryCount = 0;
    while (retryCount == 0 || retryCount < m_maxRetries) {
        if (m_controller->shouldStop()) {
            return false;
        }

        const std::vector<Sentence*> pending = currentChunk();
        if (pending.empty()) {
            return true;
        }

        bool useNativeFunctionCalling = m_agentNativeFunctionCalling != "off";
        json fileNote = loadFileNote(relInputPath);
        json messages = buildBaseMessages(backgroundText, fileNote);
        bool compactRequested = false;

        for (int turn = 0; turn < m_agentMaxTurnsPerChunk; ++turn) {
            if (approximateMessagesChars(messages) > (size_t)m_agentHardContextChars) {
                messages = buildBaseMessages(backgroundText, loadFileNote(relInputPath));
                compactRequested = false;
            }
            else if (!compactRequested && approximateMessagesChars(messages) > (size_t)m_agentSoftContextChars) {
                messages.push_back({
                    {"role", "user"},
                    {"content", "Context is close to the limit. Return a compact_context action only. Do not call tools or commit in this turn."}
                });
                compactRequested = true;
            }

            const std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
            if (!apiOpt.has_value()) {
                throw std::runtime_error("没有可用的API Key了");
            }
            const TranslationApi& currentApi = apiOpt.value();

            json payload = { {"messages", messages} };
            if (useNativeFunctionCalling && !currentApi.stream) {
                payload["tools"] = buildAgentNativeTools();
                payload["tool_choice"] = "auto";
            }

            ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);
            if (useNativeFunctionCalling && m_agentNativeFunctionCalling == "auto" && shouldFallbackFromNativeFunctionCalling(response)) {
                m_logger->warn("[线程 {}] [文件 {}] 原生函数调用不可用，自动退回文本协议。", threadId, wide2Ascii(relInputPath));
                useNativeFunctionCalling = false;
                continue;
            }

            if (!checkResponse(
                response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_controller, m_logger, retryCount, threadId, m_checkQuota
            )) {
                break;
            }

            AgentProtocolResponse protocol;
            try {
                protocol = parseAgentApiResponse(response);
            }
            catch (const std::exception& e) {
                ++retryCount;
                m_logger->warn("[线程 {}] [文件 {}] Agent 响应解析失败，第 {} 次重试。原始响应: {}\n错误: {}",
                    threadId, wide2Ascii(relInputPath), retryCount, response.content, e.what());
                break;
            }

            if (protocol.action == "tool_calls" && !protocol.calls.empty()) {
                const json toolResults = executeToolCalls(protocol.calls);
                compactRequested = false;
                if (response.hasToolCalls && useNativeFunctionCalling && response.message.is_object() && !response.message.empty()) {
                    messages.push_back(response.message);
                    for (const auto& result : toolResults) {
                        messages.push_back({
                            {"role", "tool"},
                            {"tool_call_id", result.value("id", "")},
                            {"content", result.dump(2)}
                        });
                    }
                }
                else {
                    messages.push_back({ {"role", "assistant"}, {"content", response.content} });
                    messages.push_back({
                        {"role", "user"},
                        {"content", std::string("Tool results:\n```json\n") + toolResults.dump(2) + "\n```"}
                    });
                }
                continue;
            }

            if (protocol.action == "compact_context") {
                if (protocol.summary.contains("rolling_context") && protocol.summary["rolling_context"].is_string()) {
                    backgroundText = protocol.summary["rolling_context"].get<std::string>();
                }
                else if (protocol.summary.contains("context") && protocol.summary["context"].is_string()) {
                    backgroundText = protocol.summary["context"].get<std::string>();
                }
                else if (!response.content.empty()) {
                    backgroundText = response.content;
                }
                json fileNotePatch = loadFileNote(relInputPath);
                fileNotePatch["rolling_context"] = backgroundText;
                fileNotePatch["updated_at"] = nowTimestampString();
                saveFileNote(relInputPath, fileNotePatch);
                messages = buildBaseMessages(backgroundText, loadFileNote(relInputPath));
                compactRequested = false;
                continue;
            }

            if (protocol.action == "commit") {
                int committedCount = 0;
                try {
                    applyCommit(protocol, currentApi.modelName, committedCount);
                }
                catch (const std::exception& e) {
                    ++retryCount;
                    m_logger->warn("[线程 {}] [文件 {}] Agent commit 校验失败，第 {} 次重试。错误: {}",
                        threadId, wide2Ascii(relInputPath), retryCount, e.what());
                    break;
                }
                m_logger->info("[线程 {}] [文件 {}] Agent commit 成功，提交 {} 句。", threadId, wide2Ascii(relInputPath), committedCount);
                return true;
            }

            ++retryCount;
            m_logger->warn("[线程 {}] [文件 {}] Agent 返回未知 action '{}'，第 {} 次重试。", threadId, wide2Ascii(relInputPath), protocol.action, retryCount);
            break;
        }
    }

    size_t failedCount = 0;
    for (Sentence* se : batch | std::views::filter([](const Sentence* s) { return !s->complete; })) {
        ++failedCount;
        se->pre_translated_text = "(Failed to translate)" + se->pre_processed_text;
        se->complete = true;
        ++m_completedSentences;
        m_controller->updateBar();
    }
    m_logger->error("[线程 {}] [文件 {}] Agent 批次在 {} 次重试后彻底失败，共翻译 {} / {} 句。",
        threadId, wide2Ascii(relInputPath), retryCount, batch.size() - failedCount, batch.size());
    return false;
}

void NormalJsonTranslator::runAgentFinalReconcile() {
    if (!m_agentEnabled || !m_agentFinalReconcileSingleThread) {
        return;
    }

    json rewriteQueue = loadJsonFileOrDefault(m_agentRewriteQueuePath, json::array());
    if (!rewriteQueue.is_array() || rewriteQueue.empty()) {
        return;
    }

    m_logger->info("Agent 模式开始最终单线程 reconcile，共 {} 条重翻请求。", rewriteQueue.size());
    m_agentReconciling = true;

    absl::btree_map<fs::path, std::vector<int>> fileToIds;
    for (const auto& request : rewriteQueue) {
        if (!request.is_object()) {
            continue;
        }
        const std::string file = request.value("file", "");
        const int id = request.value("id", -1);
        if (!file.empty() && id >= 0) {
            fileToIds[ascii2Wide(file)].push_back(id);
        }
    }

    for (auto& [filePath, ids] : fileToIds) {
        std::ranges::sort(ids);
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

        const fs::path cachePath = m_transCacheDir / filePath;
        if (fs::exists(cachePath)) {
            try {
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                std::ifstream ifs(cachePath, std::ios::binary);
                json cacheJson = json::parse(ifs);
                ifs.close();
                json filtered = json::array();
                if (cacheJson.is_array()) {
                    for (const auto& item : cacheJson) {
                        if (!std::ranges::contains(ids, item.value("index", -1))) {
                            filtered.push_back(item);
                        }
                    }
                }
                else {
                    filtered = cacheJson;
                }
                std::ofstream ofs(cachePath, std::ios::binary);
                ofs << filtered.dump(2);
            }
            catch (const std::exception& e) {
                m_logger->error("reconcile 清理缓存 {} 失败: {}", wide2Ascii(filePath), e.what());
                continue;
            }
        }

        try {
            processFile(filePath, 0);
        }
        catch (const std::exception& e) {
            m_logger->error("reconcile 重翻文件 {} 失败: {}", wide2Ascii(filePath), e.what());
        }
    }

    saveJsonFilePretty(m_agentRewriteQueuePath, json::array());
    m_agentReconciling = false;
    m_logger->info("Agent 模式最终单线程 reconcile 完成。");
}


#endif
// ============================================        processFile        ========================================
void NormalJsonTranslator::processFile(const fs::path& relInputPath, int threadId) {
    if (m_controller->shouldStop()) {
        return;
    }
    m_logger->debug("[线程 {}] 开始处理文件: {}", threadId, wide2Ascii(relInputPath));

    std::ifstream ifs;
    const fs::path inputPath = m_needsCombining ? (m_inputCacheDir / relInputPath) : (m_inputDir / relInputPath);
    const fs::path outputPath = m_needsCombining ? (m_outputCacheDir / relInputPath) : (m_outputDir / relInputPath);
    const fs::path cachePath = m_transCacheDir / relInputPath;
    const fs::path showNormalPath = m_projectDir / L"gt_show_normal" / relInputPath;

    createParent(outputPath);
    createParent(cachePath);
    ordered_json jSentences;
    std::vector<Sentence> sentences;

    updateAgentRunStateEntry(relInputPath, "in_progress", -1, std::format("thread-{}", threadId));


    // 解析输入文件
    try {
        ifs.open(inputPath, std::ios::binary);
        jSentences = ordered_json::parse(ifs);
        ifs.close();
        for (const auto& [index, item] : jSentences | std::views::enumerate) {
            Sentence se;
            se.index = (int)index;
            if (auto jit = item.find("name"); jit != item.end()) {
                se.nameType = NameType::Single;
                jit->get_to(se.name);
            }
            else if (jit = item.find("names"); jit != item.end()) {
                se.nameType = NameType::Multiple;
                jit->get_to(se.names);
            }
            item["message"].get_to(se.original_text);
            sentences.push_back(std::move(se));
        }
        for (auto [se1, se2] : std::views::adjacent<2>(sentences)) {
            se1.next = &se2;
            se2.prev = &se1;
        }
        for (Sentence& se : sentences) {
            preProcess(&se);
        }
    }
    catch (const json::exception& e) {
        throw std::runtime_error(std::format("[线程 {}] [文件 {}] 解析失败: {}", threadId, wide2Ascii(relInputPath), e.what()));
    }
    // 输入文件解析完毕


    // ShowNormal
    if (m_transEngine == TransEngine::ShowNormal) {
        json showNormalJson = json::array();
        for (const auto& se : sentences) {
            json showNormalObj;
            if (se.nameType == NameType::Single) {
                showNormalObj["name"] = se.name;
            }
            else if (se.nameType == NameType::Multiple) {
                showNormalObj["names"] = se.names;
            }
            showNormalObj["original_text"] = se.original_text;
            if (!se.other_info.empty()) {
                showNormalObj["other_info"] = se.other_info;
            }
            showNormalObj["pre_processed_text"] = se.pre_processed_text;
            showNormalJson.push_back(std::move(showNormalObj));
            ++m_completedSentences;
            m_controller->updateBar(); // ShowNormal
        }
        createParent(showNormalPath);
        std::ofstream ofs(showNormalPath, std::ios::binary);
        ofs << showNormalJson.dump(2);
        ofs.close();
        return;
    }
    // ShowNormal结束


    // 保存问题概览函数
    auto saveProblemOverviewFunc = [&]()
        {
            const std::string relInputPathStr = wide2Ascii(relInputPath);
            for (const auto& se : sentences) {
                if (se.problems.empty() || !se.complete) {
                    continue;
                }
                toml::ordered_table tbl;
                tbl["filename"] = relInputPathStr;
                tbl["index"] = se.index;
                if (se.nameType == NameType::Single) {
                    tbl["name"] = se.name;
                    tbl["name_preview"] = se.name_preview;
                }
                else if (se.nameType == NameType::Multiple) {
                    tbl["names"] = se.names;
                    tbl["names_preview"] = se.names_preview;
                }
                tbl["original_text"] = se.original_text;
                if (!se.other_info.empty()) {
                    tbl["other_info"] = se.other_info;
                }
                tbl["pre_processed_text"] = se.pre_processed_text;
                tbl["pre_translated_text"] = se.pre_translated_text;
                tbl["problems"] = se.problems;
                tbl["translated_by"] = se.translated_by;
                tbl["translated_preview"] = se.translated_preview;
                m_problemOverview.push_back(std::move(tbl));
            }
        };
    // 保存问题概览函数结束


    std::vector<Sentence*> toTranslate;
    // 读缓存逻辑
    {
        absl::flat_hash_map<std::string, json> cacheMap;

        auto insertJsonArrToCacheMap = [&](const json& jsonArr)
            {
                for (const auto& [index, item] : jsonArr | std::views::enumerate) {
                    std::string cacheKey = generateCacheKey(jsonArr, index);
                    cacheMap.insert({ std::move(cacheKey), item });
                }
            };

        auto usePotentialPartFileCacheToInsertCacheMap = [&](const fs::path& potentialCachePath)
            {
                try {
                    json jsonArr;
                    {
                        std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
                        ifs.open(potentialCachePath, std::ios::binary);
                        jsonArr = json::parse(ifs);
                        ifs.close();
                    }
                    insertJsonArrToCacheMap(jsonArr);
                }
                catch (const json::exception& e) {
                    throw std::runtime_error(std::format("[线程 {}] 缓存文件 {} 解析失败: {}", threadId, wide2Ascii(fs::relative(potentialCachePath, m_transCacheDir)), e.what()));
                }
            };

        std::vector<fs::path> cachePaths;

        auto readAllPotentialPartFileCache = [&](const std::wstring& cacheSpec, const fs::path& specParentDir, const std::optional<fs::path>& additionalCachePath = std::nullopt)
            {
                for (const auto& entry : fs::directory_iterator(specParentDir)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
#ifdef _WIN32
                    if (PathMatchSpecW(entry.path().filename().wstring().c_str(), cacheSpec.c_str())) {
                        if (m_needsCombining) {
                            const int diff = calculateCachePartIndexDiff(relInputPath.wstring(), entry.path().wstring());
                            if (std::abs(diff) > m_cacheSearchDistance) {
                                continue;
                            }
                        }
                        if (!std::ranges::contains(cachePaths, entry.path())) {
                            cachePaths.push_back(entry.path());
                        }
                    }
#endif
                }
                if (additionalCachePath.has_value()) {
                    cachePaths.push_back(additionalCachePath.value());
                }
                for (const auto& cp : cachePaths) {
                    usePotentialPartFileCacheToInsertCacheMap(cp);
                }
            };

        // 同名缓存优先级最高
        if (fs::exists(cachePath)) {
            cachePaths.push_back(cachePath);
        }
        if (m_transEngine != TransEngine::Rebuild) {
            if (m_needsCombining) {
                const std::optional<fs::path> additionalCachePath = [&]() -> std::optional<fs::path>
	                {
                        if (const auto it = m_splitFilePartsToJson.find(relInputPath); 
                            it != m_splitFilePartsToJson.end() && fs::exists(m_transCacheDir / it->second)) 
                        {
                            return m_transCacheDir / it->second;
                        }
                        return std::nullopt;
                    }();
                // 这个逻辑还挺耗时的
                const size_t pos = relInputPath.filename().wstring().rfind(L"_part_");
                const std::wstring orgStem = relInputPath.filename().wstring().substr(0, pos);
                const std::wstring cacheSpec = orgStem + L"_part_*.json";
                // 分割优先读分割缓存
                readAllPotentialPartFileCache(cacheSpec, m_transCacheDir / relInputPath.parent_path(), additionalCachePath);
            }
            else {
                const std::wstring cacheSpec = relInputPath.stem().wstring() + L"_part_*.json";
                // 非分割优先读整体缓存
                readAllPotentialPartFileCache(cacheSpec, m_transCacheDir / relInputPath.parent_path());
            }
        }


        // 再尽量覆盖一些边缘情况
        {
            json totalCacheJsonList = json::array();
            for (const auto& cp : cachePaths) {
                try {
                    json cacheJsonList;
                    {
                        std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
                        ifs.open(cp, std::ios::binary);
                        cacheJsonList = json::parse(ifs);
                        ifs.close();
                    }
                    totalCacheJsonList.insert(totalCacheJsonList.end(), cacheJsonList.begin(), cacheJsonList.end());
                }
                catch (const json::exception& e) {
                    throw std::runtime_error(std::format("[线程 {}] 缓存文件 {} 解析失败: {}", threadId, wide2Ascii(cp), e.what()));
                }
            }
            insertJsonArrToCacheMap(totalCacheJsonList);
        }


        for (Sentence& se : sentences) {
            if (se.complete) {
                ++m_completedSentences;
                m_controller->updateBar(); // 跳过已完成的句子
                postProcess(&se);
                continue;
            }
            const std::string key = generateCacheKey(&se);
            const auto it = cacheMap.find(key);
            if (it == cacheMap.end()) {
                toTranslate.push_back(&se);
                continue;
            }
            const auto& item = it->second;
            // 命中缓存了就把 problems 带上
            if (auto jit = item.find("problems"); jit != item.end()) {
                jit->get_to(se.problems);
            }
            if (m_transEngine != TransEngine::Rebuild && hasRetranslKey(m_retranslKeys, item, &se)) {
                toTranslate.push_back(&se);
                continue;
            }
            else {
                se.pre_translated_text = item.value("pre_translated_text", "");
                se.translated_by = item.value("translated_by", "");
                se.complete = true;
                ++m_completedSentences;
                m_controller->updateBar(); // 命中缓存
                postProcess(&se);
            }
        }

        if (!toTranslate.empty()) {
            m_logger->info("[线程 {}] [文件 {}] 共 {} 句，命中缓存/跳过 {} 句，需翻译 {} 句。", threadId, wide2Ascii(relInputPath),
                sentences.size(), sentences.size() - toTranslate.size(), toTranslate.size());
        }

        if (m_transEngine == TransEngine::Rebuild && !toTranslate.empty()) {
            const std::string notFoundSentences = toTranslate | std::views::transform([](const auto& se) { return se->original_text; })
        	        | std::views::join_with('\n') | std::ranges::to<std::string>();
            m_logger->critical("[线程 {}] [文件 {}] 有 {} 句未命中缓存，这些句子是: {}", 
                threadId, wide2Ascii(relInputPath), toTranslate.size(), notFoundSentences);
            saveCache(sentences, cachePath);
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex); // Rebuild 时这里的 saveCache 没有竞态，只有 saveProblemOverview 有
            saveProblemOverviewFunc();
            return;
        }
    }
    // 读缓存逻辑结束

    // 翻译逻辑
    if (m_transEngine != TransEngine::Rebuild && !toTranslate.empty()) {

        std::unique_ptr<py::gil_scoped_release> release = m_pythonTranslator ? std::make_unique<py::gil_scoped_release>() : nullptr;

        int batchCount = 0;
        const std::string filePathWithHash = std::format("{}{:08X}", wide2Ascii(relInputPath), calculateFileCRC64(inputPath));
        std::string backgroundText = [&]()
            {
                std::shared_lock<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
                if (const auto it = m_backgroundTextCacheMap.find(filePathWithHash); it != m_backgroundTextCacheMap.end()) {
                    return it->second;
                }
                return std::string{};
            }();

        const Sentence* pLastSentence = nullptr;
        for (auto batchView : toTranslate | std::views::chunk(m_batchSize)) {

            if (!backgroundText.empty() && pLastSentence) {
                if (batchView.front()->index - pLastSentence->index > m_batchSize) {
                    backgroundText.clear();
                }
            }
            pLastSentence = batchView.back();

            if (m_controller->shouldStop()) {
                if (!backgroundText.empty()) {
                    std::lock_guard<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
                    m_backgroundTextCacheMap[filePathWithHash] = backgroundText;
                }
                m_logger->debug("[线程 {}] [文件 {}] 已停止翻译", threadId, wide2Ascii(relInputPath));
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
                saveProblemOverviewFunc();
                updateAgentRunStateEntry(relInputPath, "pending", -1, {});
                return;
            }

            // Normal mode issues one model request per batch. Agent mode keeps the same outer
            // scheduler but replaces the inner translation step with a small multi-turn loop.
            if (m_agentEnabled) {
                translateBatchAgent(relInputPath, batchView, backgroundText, threadId);
            }
            else {
                translateBatch(relInputPath, batchView, backgroundText, threadId);
            }
            for (Sentence* se : batchView) {
                postProcess(se);
            }

            if (++batchCount % m_saveCacheInterval == 0) {
                m_logger->debug("[线程 {}] [文件 {}] 达到保存间隔，正在更新缓存文件...", threadId, wide2Ascii(relInputPath));
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
            }
        }

        std::lock_guard<std::shared_mutex> lock(m_backgroundTextCacheMapMutex);
        m_backgroundTextCacheMap.erase(filePathWithHash);
    }
    // 翻译逻辑结束


    // 最终保存缓存逻辑
    {
        std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
        m_logger->debug("[线程 {}] [文件 {}] 翻译完成，正在进行最终保存...", threadId, wide2Ascii(relInputPath));
        saveCache(sentences, cachePath);
        saveProblemOverviewFunc();
    }
    // 最终保存缓存逻辑结束


    for (auto [se, item] : std::views::zip(sentences, jSentences)) {
        if (se.nameType == NameType::Single) {
            item["name"] = se.name_preview;
        }
        else if (se.nameType == NameType::Multiple) {
            item["names"] = se.names_preview;
        }
        item["message"] = se.translated_preview;
        if (m_outputWithSrc) {
            item["src_msg"] = se.original_text;
        }
    }

    std::ofstream ofs(outputPath, std::ios::binary);
    ofs << jSentences.dump(2);
    ofs.close();
    updateAgentRunStateEntry(relInputPath, "done", sentences.empty() ? -1 : sentences.back().index, {});

    m_logger->info("[线程 {}] [文件 {}] 处理完成。", threadId, wide2Ascii(relInputPath));


    if (m_needsCombining) {
        const fs::path& originalRelFilePath = m_splitFilePartsToJson[relInputPath];
        absl::flat_hash_map<fs::path, bool>& splitFileParts = m_jsonToSplitFileParts[originalRelFilePath];
        {
            std::lock_guard<std::mutex> lock(m_outputMutex);
            splitFileParts[relInputPath] = true;
            if (
                std::ranges::any_of(splitFileParts, [](const auto& p) { return !p.second; })
                )
            {
                m_logger->debug("文件 {} 尚未全部处理完成，跳过合并。", wide2Ascii(originalRelFilePath));
                return;
            }
            m_logger->debug("开始合并 {} 的缓存文件...", wide2Ascii(originalRelFilePath));
        }
        combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
        if (m_onFileProcessed) {
            std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock); // 非常神奇,总之如果 m_onFileProcessed 是 python 侧赋值的闭包的话，这里的不 unlock 就会死锁
            if (!m_pythonTranslator) {
                lock.lock();
            }
            m_onFileProcessed(originalRelFilePath);
        }
        m_logger->debug("[线程 {}] [文件 {}] 合并处理完成。", threadId, wide2Ascii(originalRelFilePath));
    }
    else {
        if (m_onFileProcessed) {
            std::unique_lock<std::mutex> lock(m_outputMutex, std::defer_lock);
            if (!m_pythonTranslator) {
                lock.lock();
            }
            m_onFileProcessed(relInputPath);
        }
    }
}

// ================================================         run           ========================================
std::optional<std::vector<fs::path>> NormalJsonTranslator::beforeRun() {

    if (fs::exists(m_transCacheDir)) {
        try {
            fs::copy(m_transCacheDir, m_transCacheDir.parent_path() / (m_transCacheDir.filename().wstring() + L"_bak"),
                fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        }
        catch (const fs::filesystem_error& e) {
            m_logger->error("复制缓存文件夹时出现异常: {}", e.what());
        }
    }
    for (const auto& dir : { m_inputDir, m_outputDir, m_transCacheDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->debug("已创建目录: {}", wide2Ascii(dir));
        }
    }

    // 所有的json相对路径
    std::vector<fs::path> relJsonPaths;

    std::ifstream ifs;
    std::ofstream ofs;

    const fs::path nameTablePath = m_projectDir / L"人名替换表.toml";
    // 人名表处理
    {
        absl::flat_hash_map<std::string, int> jsonNameTable;
        Sentence se;

        auto insertJsonNameTable = [&](const std::string& name)
            {
                if (!name.empty()) {
                    ++jsonNameTable[name];
                }
            };

        // 收集人名和json相对路径，检查msg字段是否存在
        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }
            const fs::path relInputPath = fs::relative(entry.path(), m_inputDir);
            try {
                ifs.open(entry.path(), std::ios::binary);
                json data = json::parse(ifs);
                ifs.close();

                for (const auto& [index, item] : data | std::views::enumerate) {
                    if (!item.contains("message")) {
                        throw std::runtime_error(std::format("[文件 {}] 第 {} 个对象缺少 message 字段。", wide2Ascii(relInputPath), index));
                    }
                    ++m_totalSentences;
                    if (auto jit = item.find("name"); jit != item.end()) {
                        jit->get_to(se.name);
                        if (m_usePreDictInName) {
                            se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                        }
                        insertJsonNameTable(se.name);
                    }
                    else if (jit = item.find("names"); jit != item.end()) {
                        for (const auto& name : jit.value()) {
                            name.get_to(se.name);
                            if (m_usePreDictInName) {
                                se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                            }
                            insertJsonNameTable(se.name);
                        }
                    }
                }

                relJsonPaths.push_back(std::move(relInputPath));
            }
            catch (const json::exception& e) {
                m_logger->critical("读取文件 {} 时出错", wide2Ascii(relInputPath));
                throw std::runtime_error(e.what());
            }
        }

        if (m_totalSentences == 0) {
            throw std::runtime_error("未找到有效的 Sentence");
        }
        m_controller->makeBar(m_totalSentences, m_threadsNum);

        toml::value orgNameTable = toml::table{};
        try {
            if (fs::exists(nameTablePath)) {
                orgNameTable = toml::uparse(nameTablePath);
            }
        }
        catch (...) {
            m_logger->error("解析原人名表失败");
        }

        std::vector<std::pair<std::string, int>> jsonNameTablePairs = jsonNameTable 
    	        | std::views::transform([](const auto& pair) { return std::pair{ pair.first, pair.second }; }) | std::ranges::to<std::vector>();
        std::ranges::sort(jsonNameTablePairs, [&](const auto& a, const auto& b)
            {
                if (a.second == b.second) {
                    return a.first.length() > b.first.length();
                }
                return a.second > b.second;
            });

        toml::ordered_value newNameTable = toml::ordered_table{};
        newNameTable.comments().push_back("'原名' = [ '译名', 出现次数 ]");
        for (const std::string& key : jsonNameTablePairs | std::views::keys) {
            try {
                newNameTable[key] = toml::array{ toml::find_or(orgNameTable, key, 0, ""), jsonNameTable[key] };
            }
            catch (...) {
                newNameTable[key] = toml::array{ "", jsonNameTable[key] };
            }
        }
        ofs.open(nameTablePath, std::ios::binary);
        ofs << newNameTable;
        ofs.close();
        m_logger->info("已更新 人名替换表.toml 文件");
        if (m_transEngine == TransEngine::DumpName) {
            m_completedSentences += m_totalSentences;
            m_controller->updateBar(m_totalSentences);
            return std::nullopt;
        }
    }
    // 人名表处理完毕


    // 人名表翻译
    if (m_transEngine == TransEngine::NameTrans) {
        NameTranslator nameTranslator(m_controller, m_logger, m_apiPool, m_gptDictionary, m_onPerformApi,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang, m_maxRetries, m_apiTimeOutMs, m_checkQuota);
        nameTranslator.run(nameTablePath);
        return std::nullopt;
    }
    // 人名表翻译完毕


    // 字典生成
    if (m_transEngine == TransEngine::GenDict) {
        auto preProcessFunc = [this](Sentence* se)
            {
                this->preProcess(se);
            };
        DictionaryGenerator generator(m_controller, m_logger, m_apiPool, m_tokenizeSourceLangFunc, m_otherCacheDir,
            std::move(preProcessFunc), m_onPerformApi, m_onDictProcessed,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_maxRetries, m_threadsNum, m_apiTimeOutMs, m_checkQuota);
        const fs::path outputFilePath = m_projectDir / L"项目GPT字典-生成.toml";
        const std::vector<fs::path> inputPaths = relJsonPaths | std::views::transform([&](const auto& p) { return m_inputDir / p; }) | std::ranges::to<std::vector>();
        generator.generate(inputPaths, outputFilePath);
        return std::nullopt;
    }
    // 字典生成完毕


    // 解析人名替换表
    try {
        const auto nameTable = toml::uparse(m_projectDir / L"人名替换表.toml");

        for (const auto& [key, value] : nameTable.as_table()) {
            if (!value.is_array() || value.size() == 0) {
                continue;
            }
            const std::string transName = toml::find_or(value, 0, "");
            if (!transName.empty()) {
                m_logger->trace("发现原名 '{}' 的译名 '{}'", key, transName);
                m_nameMap.insert({ key, transName });
            }
        }
    }
    catch (const toml::exception& e) {
        m_logger->critical("解析 人名替换表.toml 时出错");
        throw std::runtime_error(e.what());
    }
    // 解析人名替换表完毕


    // 单文件分割
    {
        auto splitFunc = [&](const std::function<std::vector<ordered_json>(const ordered_json&, int)>& splitImplFunc)
            {
                if (m_splitFileNum <= 0) {
                    throw std::invalid_argument("文件分割数必须大于 0");
                }
                m_needsCombining = true;
                m_logger->info("检测到文件分割模式 ({})，开始预处理输入文件...", m_splitFile);
                for (const auto& relJsonPath : relJsonPaths) {
                    try {
                        ifs.open(m_inputDir / relJsonPath, std::ios::binary);
                        const ordered_json data = ordered_json::parse(ifs);
                        ifs.close();
                        const std::vector<ordered_json> parts = splitImplFunc(data, m_splitFileNum);
                        const std::wstring relStem = relJsonPath.parent_path() / relJsonPath.stem();
                        for (const auto& [index, part] : parts | std::views::enumerate) {
                            const fs::path relPartPath = std::format(L"{}_part_{}{}", relStem, index, relJsonPath.extension().wstring());
                            m_splitFilePartsToJson[relPartPath] = relJsonPath;
                            m_jsonToSplitFileParts[relJsonPath].insert({ relPartPath, false });
                            const fs::path partPath = m_inputCacheDir / relPartPath;
                            createParent(partPath);
                            ofs.open(partPath, std::ios::binary);
                            ofs << part.dump(2);
                            ofs.close();
                        }
                        m_logger->debug("文件 {} 已被分割成 {} 份，存入输入缓存。", wide2Ascii(relJsonPath), parts.size());

                    }
                    catch (const json::exception& e) {
                        m_logger->critical("分割文件 {} 时出错", wide2Ascii(relJsonPath));
                        throw std::runtime_error(e.what());
                    }
                }
            };
        if (m_splitFile == "Equal") {
            splitFunc(splitJsonArrayEqual);
        }
        else if (m_splitFile == "Num") {
            splitFunc(splitJsonArrayNum);
        }
        else if (m_splitFile != "No") {
            throw std::invalid_argument(std::format("未知的文件分割模式: {}, 请使用 'No', 'Equal', 'Num'", m_splitFile));
        }
    }
    // 单文件分割完毕

    // 分发文件
    std::vector<fs::path> relFilePaths = m_needsCombining ? (m_splitFilePartsToJson | std::views::keys | std::ranges::to<std::vector>())
        : std::move(relJsonPaths);

    if (m_sortMethod == "size") {
        std::ranges::sort(relFilePaths, [&](const fs::path& a, const fs::path& b)
            {
                return m_needsCombining ? (fs::file_size(m_inputCacheDir / a) > fs::file_size(m_inputCacheDir / b)) :
                    (fs::file_size(m_inputDir / a) > fs::file_size(m_inputDir / b));
            });
    }
    else if (m_sortMethod == "name") {
#ifdef _WIN32
        std::ranges::sort(relFilePaths, [](const fs::path& a, const fs::path& b)
            {
                return str2Lower(a) < str2Lower(b);
            });
#else
        std::ranges::sort(relFilePaths);
#endif
    }
    else {
        throw std::invalid_argument(std::format("未知的排序模式: {}", m_sortMethod));
    }

    if (m_agentEnabled) {
        m_agentKnownRelFiles = relFilePaths;
        createParent(m_agentRunStatePath);
        fs::create_directories(m_agentFileNotesDir);
        const json currentAgentConfig = {
            {"threads_num", m_threadsNum},
            {"split_file", m_splitFile},
            {"split_file_num", m_splitFileNum},
            {"chunk_size", m_agentChunkSize},
            {"max_turns_per_chunk", m_agentMaxTurnsPerChunk},
            {"soft_context_chars", m_agentSoftContextChars},
            {"hard_context_chars", m_agentHardContextChars},
            {"lookahead_lines", m_agentLookaheadLines},
            {"search_result_limit", m_agentSearchResultLimit},
            {"allow_cross_file_search", m_agentAllowCrossFileSearch},
            {"native_function_calling", m_agentNativeFunctionCalling},
            {"final_reconcile_single_thread", m_agentFinalReconcileSingleThread},
            {"rewrite_mode", m_agentRewriteMode}
        };
        const std::string configSignature = currentAgentConfig.dump();

        json runState = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
        const json previousAgentConfig = runState.contains("config") && runState["config"].is_object() ? runState["config"] : json::object();
        const bool hasPreviousConfig = !previousAgentConfig.empty();
        const bool threadsChanged = hasPreviousConfig && previousAgentConfig.value("threads_num", m_threadsNum) != m_threadsNum;
        const bool workLayoutChanged = !hasPreviousConfig
            || previousAgentConfig.value("split_file", m_splitFile) != m_splitFile
            || previousAgentConfig.value("split_file_num", m_splitFileNum) != m_splitFileNum
            || previousAgentConfig.value("chunk_size", m_agentChunkSize) != m_agentChunkSize;

        absl::flat_hash_map<std::string, json> oldEntries;
        if (runState.contains("files") && runState["files"].is_array()) {
            for (const auto& item : runState["files"]) {
                if (!item.is_object()) {
                    continue;
                }
                const std::string relFile = item.value("file", "");
                if (!relFile.empty()) {
                    oldEntries.insert_or_assign(relFile, item);
                }
            }
        }

        int preservedDoneCount = 0;
        int requeuedCount = 0;
        json normalizedFiles = json::array();
        const std::string updatedAt = nowTimestampString();
        for (const auto& relFilePath : relFilePaths) {
            const std::string relFileStr = wide2Ascii(relFilePath);
            json entry = {
                {"file", relFileStr},
                {"status", "pending"},
                {"lease_owner", ""},
                {"last_committed_index", -1},
                {"updated_at", updatedAt}
            };

            const bool artifactDone = hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
            if (const auto it = oldEntries.find(relFileStr); it != oldEntries.end()) {
                const json& oldEntry = it->second;
                const std::string oldStatus = oldEntry.value("status", "pending");
                if (!workLayoutChanged) {
                    entry["last_committed_index"] = oldEntry.value("last_committed_index", -1);
                }
                if (artifactDone && oldStatus == "done") {
                    entry["status"] = "done";
                    ++preservedDoneCount;
                }
                else {
                    if (oldStatus == "in_progress" || oldStatus == "done") {
                        ++requeuedCount;
                    }
                    if (workLayoutChanged) {
                        entry["last_committed_index"] = -1;
                    }
                }
            }
            else if (artifactDone) {
                entry["status"] = "done";
                ++preservedDoneCount;
            }

            normalizedFiles.push_back(std::move(entry));
        }

        runState = {
            {"config_signature", configSignature},
            {"config", currentAgentConfig},
            {"updated_at", updatedAt},
            {"files", normalizedFiles}
        };
        saveJsonFilePretty(m_agentRunStatePath, runState);

        if (hasPreviousConfig) {
            if (workLayoutChanged) {
                m_logger->info("Agent 运行配置发生工作单元变化，已重建调度列表并保留 {} 个已完成工作单元。", preservedDoneCount);
            }
            else if (threadsChanged) {
                m_logger->info("Agent 线程数发生变化，已清理旧 lease，并保留 {} 个已完成工作单元。", preservedDoneCount);
            }
            else if (requeuedCount > 0) {
                m_logger->info("Agent 已恢复上次未完成的进度，重新排队 {} 个工作单元。", requeuedCount);
            }
        }

        if (m_needsCombining) {
            for (const auto& item : runState["files"]) {
                const fs::path relFilePath = ascii2Wide(item.value("file", ""));
                const bool isDone = item.value("status", "pending") == "done"
                    && hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
                if (const auto it = m_splitFilePartsToJson.find(relFilePath); it != m_splitFilePartsToJson.end()) {
                    m_jsonToSplitFileParts[it->second][relFilePath] = isDone;
                }
            }
        }

        saveJsonFilePretty(m_agentSearchCatalogPath, json{
            {"updated_at", nowTimestampString()},
            {"files", relFilePaths | std::views::transform([](const fs::path& p) { return wide2Ascii(p); }) | std::ranges::to<std::vector>()}
        });
        if (!fs::exists(m_agentTermLedgerPath)) {
            saveJsonFilePretty(m_agentTermLedgerPath, json::object());
        }
        if (!fs::exists(m_agentRewriteQueuePath)) {
            saveJsonFilePretty(m_agentRewriteQueuePath, json::array());
        }
    }

    return relFilePaths;
}

void NormalJsonTranslator::afterRun() {
    // 问题概览
    if (m_problemOverview.as_array().empty()) {
        m_logger->info("\n\n```\n无问题概览\n```\n");
    }
    else {
        std::ofstream ofs;
        ofs.open(m_projectDir / L"翻译问题概览.toml", std::ios::binary);
        ofs << toml::format("problemOverview", m_problemOverview);
        ofs.close();
        ofs.open(m_projectDir / L"翻译问题概览.json", std::ios::binary);
        ofs << toml2Json(m_problemOverview).dump(2);
        ofs.close();
        m_logger->debug("已生成 翻译问题概览.json 和 翻译问题概览.toml 文件");

        absl::btree_map<std::string, absl::flat_hash_set<std::string>> problemMap;
        for (const auto& [problem, filename] : m_problemOverview.as_array()
            | std::views::transform([](const auto& tbl)
                {
                    const auto& problemsArr = tbl.at("problems").as_array();
                    const auto problemsWithFileNameView = problemsArr | std::views::transform([&](const auto& prob)
                        {
                            return std::make_pair(prob.as_string(), tbl.at("filename").as_string());
                        });
                    return problemsWithFileNameView;
                }) 
            | std::views::join)
        {
            problemMap[problem].insert(filename);
        }

        std::string problemOverviewStr = "\n\n```\n问题概览:\n";
        size_t problemCount = 0;
        for (const auto& [problem, files] : problemMap) {
            std::string fileStr = "(";
            size_t fileCount = 0;
            for (const auto& file : files) {
                if (fileCount == 3) {
                    break;
                }
                fileStr += file + ", ";
                ++fileCount;
            }
            if (fileCount == files.size()) {
                fileStr.pop_back();
                fileStr.pop_back();
                fileStr += ")";
            }
            else {
                fileStr += "...)";
            }
            problemOverviewStr += std::format("{}. {}  |  {}\n", ++problemCount, problem, fileStr);
        }
        m_logger->error("{}问题概览结束\n```\n", problemOverviewStr);
    }
    // 问题概览完毕

    // 背景文本缓存
    {
        try {
            createParent(m_backgroundTextCachePath);
            json j = m_backgroundTextCacheMap;
            std::ofstream ofs(m_backgroundTextCachePath, std::ios::binary);
            ofs << j.dump(2);
            ofs.close();
            m_logger->debug("背景文本缓存已保存至 {}", wide2Ascii(m_backgroundTextCachePath));
        }
        catch (...) {
            m_logger->error("背景文本缓存 {} 保存失败", wide2Ascii(m_backgroundTextCachePath));
        }
    }
    // 背景文本缓存完毕

    if (m_needsCombining) {
        fs::remove_all(m_inputCacheDir);
        fs::remove_all(m_outputCacheDir);
    }
    if (!m_controller->shouldStop() && m_transEngine == TransEngine::Rebuild && m_completedSentences != m_totalSentences) {
        m_logger->critical("重建过程中有句子未命中缓存 ({}/{} lines)，请检查日志以定位问题。", m_completedSentences.load(), m_totalSentences);
    }
}

void NormalJsonTranslator::process(std::vector<fs::path> relFilePaths) {
    if (m_agentEnabled && !m_agentReconciling) {
        json runState = loadJsonFileOrDefault(m_agentRunStatePath, json::object());
        absl::flat_hash_map<std::string, std::string> statusByFile;
        if (runState.contains("files") && runState["files"].is_array()) {
            for (const auto& item : runState["files"]) {
                if (!item.is_object()) {
                    continue;
                }
                const std::string relFile = item.value("file", "");
                if (!relFile.empty()) {
                    statusByFile.insert_or_assign(relFile, item.value("status", "pending"));
                }
            }
        }

        if (m_needsCombining) {
            for (const auto& [originalRelFilePath, splitFileParts] : m_jsonToSplitFileParts) {
                const bool allDone = std::ranges::all_of(splitFileParts, [&](const auto& partState) {
                    return partState.second
                        && hasAgentWorkUnitArtifacts(partState.first, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
                });
                if (!allDone || fs::exists(m_outputDir / originalRelFilePath)) {
                    continue;
                }
                m_logger->info("Agent 恢复已完成的分割输出合并: {}", wide2Ascii(originalRelFilePath));
                combineOutputFiles(originalRelFilePath, splitFileParts, m_outputCacheDir, m_outputDir, m_logger);
            }
        }

        std::vector<fs::path> pendingFilePaths;
        int skippedDoneCount = 0;
        for (const auto& relFilePath : relFilePaths) {
            const std::string relFileStr = wide2Ascii(relFilePath);
            const auto statusIt = statusByFile.find(relFileStr);
            const bool isDone = statusIt != statusByFile.end()
                && statusIt->second == "done"
                && hasAgentWorkUnitArtifacts(relFilePath, m_outputDir, m_outputCacheDir, m_transCacheDir, m_needsCombining);
            if (isDone) {
                ++skippedDoneCount;
                continue;
            }
            pendingFilePaths.push_back(relFilePath);
        }

        if (skippedDoneCount > 0) {
            m_logger->info("Agent 恢复模式跳过 {} 个已完成工作单元。", skippedDoneCount);
        }
        relFilePaths = std::move(pendingFilePaths);
    }

    if (relFilePaths.empty()) {
        m_logger->info("没有需要重新调度的文件任务。");
        runAgentFinalReconcile();
        return;
    }

    std::vector<std::future<void>> results;
    m_threadPool.resize(std::min(m_threadsNum, (int)relFilePaths.size()));
    for (const auto& filePath : relFilePaths) {
        results.emplace_back(m_threadPool.push([=](const int id)
            {
                m_controller->addThreadNum();
                this->processFile(filePath, id);
                m_controller->reduceThreadNum();
            }));
    }
    m_logger->info("已将 {} 个文件任务分配到线程池，等待处理完成...", results.size());
    waitForThreads(m_threadPool, results);
    runAgentFinalReconcile();
}

void NormalJsonTranslator::run() {
    NormalJsonTranslator::init();
    std::optional<std::vector<fs::path>> relFilePathsOpt = NormalJsonTranslator::beforeRun();
    if (!relFilePathsOpt.has_value()) {
        return;
    }
    NormalJsonTranslator::process(std::move(relFilePathsOpt.value()));
    NormalJsonTranslator::afterRun();
}
