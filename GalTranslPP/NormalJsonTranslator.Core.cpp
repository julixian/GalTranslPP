module;

#define PYBIND11_HEADERS
#define SOL2_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>
#include <proxy/proxy.h>
#include <toml.hpp>

module NormalJsonTranslator;

import ConditionTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

namespace
{
#define GPP_REQUIRE_CONFIG(CONDITION, KEY, VALUE, REQUIREMENT) \
    do { \
        if (!(CONDITION)) { \
            const std::string gppConfigValue = std::format("{}", VALUE); \
            const std::string gppRequirement = std::format("{}", REQUIREMENT); \
            throw std::invalid_argument(gppTr( \
                    "validateNormalJsonCoreConfig", \
                    "配置项 %1 无效: 当前值 %2，要求%3") \
                .arg(KEY) \
                .arg(gppConfigValue) \
                .arg(gppRequirement) \
                .toStdString()); \
        } \
    } while (false)

    struct NormalJsonCoreConfig {
        TransEngine transEngine{};
        std::string_view sortMethod;
        std::string_view splitFileMethod;
        std::string_view problemOverviewFormat;
        bool agentEnabled{};
        int batchSize{};
        int threadsNum{};
        int nameTransBatchSize{};
        int splitFileNum{};
        int repeatedBlockMinSize{};
        int cacheSearchDistance{};
        int saveCacheInterval{};
        int maxRequestCount{};
        int contextHistorySize{};
        int inputBlockMaxLines{};
        int problemMaxLines{};
        int glossaryMaxLines{};
        int agentMaxTurnsPerChunk{};
        int agentCompactContextThresholdBytes{};
        int agentSearchResultLimit{};
        int agentContextLinesLimit{};
    };

    void validateNormalJsonCoreConfig(const NormalJsonCoreConfig& config)
    {
    	auto isOneOf = [](std::string_view value, std::initializer_list<std::string_view> candidates)
            {
                return std::ranges::find(candidates, value) != candidates.end();
            };

        GPP_REQUIRE_CONFIG(
            config.batchSize > 0,
            "common.numPerRequestTranslate",
            config.batchSize,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.threadsNum > 0,
            "common.threadsNum",
            config.threadsNum,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.nameTransBatchSize > 0,
            "common.numPerRequestNameTranslate",
            config.nameTransBatchSize,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            isOneOf(config.sortMethod, { "name", "size" }),
            "common.sortMethod",
            config.sortMethod,
            gppTr(
                "validateNormalJsonCoreConfig",
                "为 name 或 size")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            isOneOf(config.splitFileMethod, { "No", "Num", "Equal" }),
            "common.split.method",
            config.splitFileMethod,
            gppTr(
                "validateNormalJsonCoreConfig",
                "为 No、Num 或 Equal")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            isOneOf(config.problemOverviewFormat, { "toml", "json" }),
            "common.problemOverviewFormat",
            config.problemOverviewFormat,
            gppTr(
                "validateNormalJsonCoreConfig",
                "为 toml 或 json")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.splitFileNum > 0,
            "common.split.num",
            config.splitFileNum,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.repeatedBlockMinSize >= 2,
            "common.repeatedBlock.minSize",
            config.repeatedBlockMinSize,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于等于 2")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.cacheSearchDistance >= 0,
            "common.split.cacheSearchDistance",
            config.cacheSearchDistance,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于等于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.saveCacheInterval > 0,
            "common.saveCacheInterval",
            config.saveCacheInterval,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.maxRequestCount > 0,
            "common.maxRequestCount",
            config.maxRequestCount,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.contextHistorySize >= 0,
            "common.contextHistorySize",
            config.contextHistorySize,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于等于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.inputBlockMaxLines > 0,
            "common.log.inputBlockMaxLines",
            config.inputBlockMaxLines,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.problemMaxLines > 0,
            "common.log.problemMaxLines",
            config.problemMaxLines,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.glossaryMaxLines > 0,
            "common.log.glossaryMaxLines",
            config.glossaryMaxLines,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());

        if (!config.agentEnabled) {
            return;
        }

        GPP_REQUIRE_CONFIG(
            config.agentMaxTurnsPerChunk > 0,
            "common.agent.maxTurnsPerChunk",
            config.agentMaxTurnsPerChunk,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于 0")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.agentSearchResultLimit >= 1,
            "common.agent.searchResultLimit",
            config.agentSearchResultLimit,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于等于 1")
                .toStdString());
        GPP_REQUIRE_CONFIG(
            config.agentContextLinesLimit >= 0,
            "common.agent.contextLinesLimit",
            config.agentContextLinesLimit,
            gppTr(
                "validateNormalJsonCoreConfig",
                "大于等于 0")
                .toStdString());
        if (config.transEngine != TransEngine::GenDict) {
            GPP_REQUIRE_CONFIG(
                config.agentCompactContextThresholdBytes > 0,
                "common.agent.compactContextThresholdBytes",
                config.agentCompactContextThresholdBytes,
                gppTr(
                    "validateNormalJsonCoreConfig",
                    "大于 0")
                    .toStdString());
        }
    }

#undef GPP_REQUIRE_CONFIG
}

NormalJsonTranslator::~NormalJsonTranslator()
{
    m_logger->info(gppTr(
        "NormalJsonTranslator.~NormalJsonTranslator",
        "所有任务已完成！NormalJsonTranslator 结束，总耗时 %1 秒")
        .arg(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_startTimePoint)
            .count() / 1000.0, 0, 'f', 2)
        .toStdString());
}

NormalJsonTranslator::NormalJsonTranslator(
    const fs::path& projectDir,
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    const std::optional<fs::path>& inputDir,
    const std::optional<fs::path>& inputCacheDir,
    const std::optional<fs::path>& outputDir,
    const std::optional<fs::path>& outputCacheDir
) :
    m_controller(controller), m_logger(logger), m_projectDir(projectDir),
    m_luaManager(std::make_unique<LuaManager>(logger)), m_pythonManager(std::make_unique<PythonManager>(logger))
{
    m_logger->info(gppTr(
        "NormalJsonTranslator.NormalJsonTranslator",
        "GalTransl++ NormalJsonTranslator 启动...")
        .toStdString());
    m_inputDir = inputDir.value_or(m_projectDir / L"gt_input");
    m_inputCacheDir = inputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_input_cache");
    m_outputDir = outputDir.value_or(m_projectDir / L"gt_output");
    m_outputCacheDir = outputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_output_cache");
    m_transCacheDir = m_projectDir / transCacheDirName;
    m_otherCacheDir = m_projectDir / otherCacheDirName;
    m_nameTablePath = m_projectDir / L"NameTable.toml";
    m_rollingContextCachePath = m_otherCacheDir / L"rollingContextCache.json";
    m_agentRootDir = m_otherCacheDir / L"agent";
    m_agentTermLedgerPath = m_agentRootDir / L"term_ledger.json";
    m_agentFileNotesDir = m_agentRootDir / L"file_notes";
    try {
        if (fs::exists(m_rollingContextCachePath)) {
            parseJson(m_rollingContextCachePath).get_to(m_rollingContextCacheMap);
        }
        else {
            m_logger->debug(gppTr("NormalJsonTranslator.NormalJsonTranslator", "未找到 rolling context 缓存文件 [%1]")
                .arg(wide2Ascii(m_rollingContextCachePath))
                .toStdString());
        }
    }
    catch (...) {
        m_logger->error(gppTr("NormalJsonTranslator.NormalJsonTranslator", "读取 rolling context 缓存文件 [%1] 失败")
            .arg(wide2Ascii(m_rollingContextCachePath))
            .toStdString());
    }
}

void NormalJsonTranslator::normalJsonInit()
{
    const fs::path configPath = m_projectDir / L"Config.toml";
    try {
        const auto configData = toml::uparse(configPath);

        const std::string transEngineStr = toml::find_or(configData, "plugins", "transEngine", "ForGalTsv");
        if (transEngineStr == "ForGalJson") {
            m_transEngine = TransEngine::ForGalJson;
        }
        else if (transEngineStr == "ForGalTsv") {
            m_transEngine = TransEngine::ForGalTsv;
        }
        else if (transEngineStr == "ForNovelTsv") {
            m_transEngine = TransEngine::ForNovelTsv;
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
            throw std::runtime_error(gppTr(
                "NormalJsonTranslator.normalJsonInit",
                "无效的 TransEngine: %1")
                .arg(transEngineStr)
                .toStdString());
        }

        const auto pluginConfigData = toml::uparse(filePluginConfigPath / L"NormalJson.toml");
        m_outputWithSrc = parseToml<bool>(configData, pluginConfigData, "plugins.NormalJson.outputWithSrc");

        m_batchSize = toml::find_or(configData, "common", "numPerRequestTranslate", 16);
        m_threadsNum = toml::find_or(configData, "common", "threadsNum", 5);
        m_nameTransBatchSize = toml::find_or(configData, "common", "numPerRequestNameTranslate", 50);
        m_sortMethod = toml::find_or(configData, "common", "sortMethod", "size");
        m_targetLang = toml::find_or(configData, "common", "targetLang", "zh-cn");
        m_splitFileMethod = toml::find_or(configData, "common", "split", "method", "No");
        m_problemOverviewFormat = toml::find_or(configData, "common", "problemOverviewFormat", "json");
        m_splitFileNum = toml::find_or(configData, "common", "split", "num", 10);
        m_reuseRepeatedBlocks = toml::find_or(configData, "common", "repeatedBlock", "enabled", false);
        m_repeatedBlockMinSize = toml::find_or(configData, "common", "repeatedBlock", "minSize", 5);
        m_cacheSearchDistance = toml::find_or(configData, "common", "split", "cacheSearchDistance", 5);
        m_saveCacheInterval = toml::find_or(configData, "common", "saveCacheInterval", 1);
        m_linebreakSymbol = toml::find_or(configData, "common", "linebreakSymbol", "auto");
        m_maxRequestCount = toml::find_or(configData, "common", "maxRequestCount", 4);
        m_contextHistorySize = toml::find_or(configData, "common", "contextHistorySize", 8);
        m_inputBlockMaxLines = toml::find_or(configData, "common", "log", "inputBlockMaxLines", 10);
        m_problemMaxLines = toml::find_or(configData, "common", "log", "problemMaxLines", 3);
        m_glossaryMaxLines = toml::find_or(configData, "common", "log", "glossaryMaxLines", 5);
        m_smartRetry = toml::find_or(configData, "common", "smartRetry", false);
        m_checkQuota = toml::find_or(configData, "common", "checkQuota", true);
        m_retransAllWhenFail = toml::find_or(configData, "common", "retransAllWhenFail", false);
        m_agentEnabled = toml::find_or(configData, "common", "agent", "enabled", false);
        m_agentMaxTurnsPerChunk = toml::find_or(configData, "common", "agent", "maxTurnsPerChunk", 20);
        m_agentCompactContextThresholdBytes = toml::find_or(configData, "common", "agent", "compactContextThresholdBytes", 150000);
        m_agentSearchResultLimit = toml::find_or(configData, "common", "agent", "searchResultLimit", 80);
        m_agentContextLinesLimit = toml::find_or(configData, "common", "agent", "contextLinesLimit", 20);
        const std::string projectNotePathStr = toml::find_or(configData, "common", "agent", "projectNotePath", "ProjectNote.md");
        fs::path projectNotePath = ascii2Wide(projectNotePathStr);
        if (projectNotePath.is_relative()) {
            projectNotePath = m_projectDir / projectNotePath;
        }
        if (fs::exists(projectNotePath)) {
            m_agentProjectNotePath = fs::canonical(projectNotePath);
            m_logger->info(gppTr(
                "NormalJsonTranslator.normalJsonInit",
                "ProjectNote 路径已注册: [%1]")
                .arg(wide2Ascii(m_agentProjectNotePath.value()))
                .toStdString());
        }
        if (m_agentEnabled)
        {
            if (m_transEngine != TransEngine::ForGalTsv &&
                m_transEngine != TransEngine::ForNovelTsv &&
                m_transEngine != TransEngine::GenDict)
            {
                m_agentEnabled = false;
                m_logger->warn(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                        "Agent 模式在 TransEngine %1 下已自动关闭")
                    .arg(transEngineStr)
                    .toStdString());
            }
            else {
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "Agent 模式已启用")
                    .toStdString());
            }
        }

        validateNormalJsonCoreConfig({
            .transEngine = m_transEngine,
            .sortMethod = m_sortMethod,
            .splitFileMethod = m_splitFileMethod,
            .problemOverviewFormat = m_problemOverviewFormat,
            .agentEnabled = m_agentEnabled,
            .batchSize = m_batchSize,
            .threadsNum = m_threadsNum,
            .nameTransBatchSize = m_nameTransBatchSize,
            .splitFileNum = m_splitFileNum,
            .repeatedBlockMinSize = m_repeatedBlockMinSize,
            .cacheSearchDistance = m_cacheSearchDistance,
            .saveCacheInterval = m_saveCacheInterval,
            .maxRequestCount = m_maxRequestCount,
            .contextHistorySize = m_contextHistorySize,
            .inputBlockMaxLines = m_inputBlockMaxLines,
            .problemMaxLines = m_problemMaxLines,
            .glossaryMaxLines = m_glossaryMaxLines,
            .agentMaxTurnsPerChunk = m_agentMaxTurnsPerChunk,
            .agentCompactContextThresholdBytes = m_agentCompactContextThresholdBytes,
            .agentSearchResultLimit = m_agentSearchResultLimit,
            .agentContextLinesLimit = m_agentContextLinesLimit
            });

        m_usePreDictInName = toml::find_or(configData, "dictionary", "usePreDictInName", false);
        m_usePostDictInName = toml::find_or(configData, "dictionary", "usePostDictInName", false);
        m_usePreDictInMsg = toml::find_or(configData, "dictionary", "usePreDictInMsg", true);
        m_usePostDictInMsg = toml::find_or(configData, "dictionary", "usePostDictInMsg", true);
        m_useGptDictToReplaceName = toml::find_or(configData, "dictionary", "useGPTDictToReplaceName", false);
        const std::string defaultDictFolderStr = toml::find_or(configData, "dictionary", "defaultDictFolder", "BaseConfig/Dicts");
        const fs::path defaultDictFolderPath = ascii2Wide(defaultDictFolderStr);

    	auto resolveDictPathsFunc = [&](const std::string& dictType)
            {
                std::vector<fs::path> dictPaths;
                const auto dictFileNamesOpt = toml::find<
                    std::optional<std::vector<std::string>>
                >(configData, "dictionary", dictType + "Dicts");
                if (!dictFileNamesOpt.has_value()) {
                    return dictPaths;
                }
                for (const auto& dictFileName : dictFileNamesOpt.value()) {
                    fs::path dictPath = ascii2Wide(dictFileName);
                    if (dictPath.is_absolute()) {
                        if (fs::exists(dictPath)) {
                            dictPaths.push_back(dictPath);
                        }
                        else {
                            m_logger->warn(gppTr("NormalJsonTranslator.normalJsonInit",
                                "未找到字典文件 [%1]，已忽略")
                                .arg(dictFileName).toStdString());
                        }
                    }
                    else {
                        dictPath = m_projectDir / dictPath;
                        if (fs::exists(dictPath)) {
                            dictPaths.push_back(dictPath);
                        }
                        else {
                            dictPath = defaultDictFolderPath / ascii2Wide(dictType) / ascii2Wide(dictFileName);
                            if (fs::exists(dictPath)) {
                                dictPaths.push_back(dictPath);
                            }
                            else {
                                m_logger->warn(gppTr("NormalJsonTranslator.normalJsonInit",
                                    "未找到字典文件 [%1]，已忽略")
                                    .arg(dictFileName).toStdString());
                            }
                        }
                    }
                }
                return dictPaths;
            };

        auto loadDictsFunc = [&]<typename DictionaryType>(const std::string& dictType, DictionaryType& dict)
            {
                for (const fs::path& dictPath : resolveDictPathsFunc(dictType)) {
                    const fs::path canonicalPath = fs::canonical(dictPath);
                    dict->loadFromFile(canonicalPath);
                    if (dictType == "gpt") {
                        if (!std::ranges::contains(m_gptDictionaryPaths, canonicalPath)) {
                            m_gptDictionaryPaths.push_back(canonicalPath);
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

        // 这些模式需要初始化 Api 池和提示词。
        if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
            m_apiStrategy = toml::find_or(configData, "backend", "apiStrategy", "random");
            if (m_apiStrategy != "random" && m_apiStrategy != "fallback") {
                throw std::invalid_argument(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "apiStrategy 必须为 random 或 fallback")
                    .toStdString());
            }
            int apiTimeOutSecond = toml::find_or(configData, "backend", "apiTimeout", 300);
            m_apiTimeOutMs = apiTimeOutSecond * 1000;

            const auto apisArr = toml::find<
                std::vector<toml::table>
            >(configData, "backend", "apis");

            std::vector<TranslationApi> apis;
            for (const auto& [apiIndex, apiTbl] : apisArr | std::views::enumerate) {
                if (apiTbl.contains("enable") && !apiTbl.at("enable").as_boolean()) {
                    continue;
                }
                TranslationApi api;
                if (apiTbl.contains("protocol") && apiTbl.at("protocol").is_string()) {
                    api.protocol = parseApiProtocol(apiTbl.at("protocol").as_string());
                }
                else {
                    api.protocol = ApiProtocol::OpenAI;
                    m_logger->warn(gppTr("NormalJsonTranslator.normalJsonInit",
                        "backend.apis[%1] 未找到 Api 协议字段，默认使用 OpenAI 协议").arg(apiIndex).toStdString());
                }
                if (apiTbl.contains("apiurl") && !apiTbl.at("apiurl").as_string().empty()) {
                    api.apiurl = cvt2StdApiUrl(apiTbl.at("apiurl").as_string(), api.protocol);
                }
                else {
                    m_logger->warn(gppTr("NormalJsonTranslator.normalJsonInit",
                        "backend.apis[%1] apiurl 为空，已忽略").arg(apiIndex).toStdString());
                    continue;
                }
                if (apiTbl.contains("modelName") && !apiTbl.at("modelName").as_string().empty()) {
                    api.modelName = apiTbl.at("modelName").as_string();
                }
                else if (m_transEngine == TransEngine::Sakura) {
                    api.modelName = "sakura";
                }
                else {
                    m_logger->warn(gppTr("NormalJsonTranslator.normalJsonInit",
                        "backend.apis[%1] modelName 为空且不是 Sakura TransEngine，已忽略").arg(apiIndex).toStdString());
                    continue;
                }
                api.stream = apiTbl.contains("stream") && apiTbl.at("stream").as_boolean();
                api.thinkingLevel = "off";
                if (apiTbl.contains("thinkingLevel")) {
                    api.thinkingLevel = apiTbl.at("thinkingLevel").as_string();
                }
                if (apiTbl.contains("extraHeadersEnable") &&
                    apiTbl.at("extraHeadersEnable").as_boolean() &&
                    apiTbl.contains("extraHeaders"))
                {
                    for (const auto& [key, value] : apiTbl.at("extraHeaders").as_table()) {
                        if (value.is_string()) {
                            api.extraHeaders[key] = value.as_string();
                        }
                        else {
                            api.extraHeaders[key] = toml2Json(value).dump();
                        }
                    }
                }
                if (apiTbl.contains("extraBodyEnable") &&
                    apiTbl.at("extraBodyEnable").as_boolean() &&
                    apiTbl.contains("extraBody") &&
                    apiTbl.at("extraBody").is_table())
                {
                    api.extraBody = toml2Json(apiTbl.at("extraBody"));
                }
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
                bool hasApiKey = false;
                if (apiTbl.contains("apikeys")) {
                    for (const auto& apiKeyValue : apiTbl.at("apikeys").as_array()) {
                        const std::string& apiKey = apiKeyValue.as_string();
                        if (apiKey.empty()) {
                            continue;
                        }
                        TranslationApi apiWithKey = api;
                        apiWithKey.apikey = apiKey;
                        apis.push_back(std::move(apiWithKey));
                        hasApiKey = true;
                    }
                }
                if (!hasApiKey && m_transEngine == TransEngine::Sakura) {
                    TranslationApi apiWithKey = api;
                    apiWithKey.apikey = "sk-sakura";
                    apis.push_back(std::move(apiWithKey));
                }
            }
            if (apis.empty()) {
                throw std::invalid_argument(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "找不到可用的 Api key")
                    .toStdString());
            }
            else {
                m_apiPool = std::make_unique<ApiPool>(m_logger);
                m_apiPool->loadApis(std::move(apis));
            }

            const fs::path projectPromptPath = m_projectDir / L"Prompt.toml";
            const bool hasProjectPrompt = fs::exists(projectPromptPath);
            const bool hasDefaultPrompt = fs::exists(defaultPromptPath);
            if (!hasProjectPrompt && !hasDefaultPrompt) {
                throw std::runtime_error(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "找不到 Prompt.toml 文件")
                    .toStdString());
            }

            const auto projectPromptData = hasProjectPrompt ? toml::uparse(projectPromptPath) : toml::value{};
            const auto defaultPromptData = hasDefaultPrompt ? toml::uparse(defaultPromptPath) : toml::value{};

            const auto readPromptString = [&](const std::string& key) -> std::string
                {
                    if (hasProjectPrompt && projectPromptData.contains(key)) {
                        return projectPromptData.at(key).as_string();
                    }
                    if (hasDefaultPrompt && defaultPromptData.contains(key)) {
                        return defaultPromptData.at(key).as_string();
                    }
                    throw std::invalid_argument(gppTr(
                        "NormalJsonTranslator.normalJsonInit",
                        "Prompt.toml 中缺少 %1 键")
                        .arg(key)
                        .toStdString());
                };

            if (m_transEngine == TransEngine::GenDict) {
                m_systemPrompt = readPromptString("GENDICT_SYSTEM");
                m_userPrompt = readPromptString("GENDICT_USER");
                if (m_agentEnabled) {
                    m_genDictReviewSystemPrompt = readPromptString("GENDICT_REVIEW_SYSTEM");
                    m_genDictReviewUserPrompt = readPromptString("GENDICT_REVIEW_USER");
                }
            }
            else {
                std::string systemPromptKey;
                std::string userPromptKey;

                switch (m_transEngine)
                {
                case TransEngine::ForGalJson:
                    systemPromptKey = "FORGALJSON_SYSTEM";
                    userPromptKey = "FORGALJSON_USER";
                    break;
                case TransEngine::ForGalTsv:
                    systemPromptKey = m_agentEnabled ? "FORGALTSV_AGENT_SYSTEM" : "FORGALTSV_SYSTEM";
                    userPromptKey = m_agentEnabled ? "FORGALTSV_AGENT_USER" : "FORGALTSV_USER";
                    break;
                case TransEngine::ForNovelTsv:
                    systemPromptKey = m_agentEnabled ? "FORNOVELTSV_AGENT_SYSTEM" : "FORNOVELTSV_SYSTEM";
                    userPromptKey = m_agentEnabled ? "FORNOVELTSV_AGENT_USER" : "FORNOVELTSV_USER";
                    break;
                case TransEngine::Sakura:
                    systemPromptKey = "SAKURA_SYSTEM";
                    userPromptKey = "SAKURA_USER";
                    break;
                case TransEngine::NameTrans:
                    systemPromptKey = "NAMETRANS_SYSTEM";
                    userPromptKey = "NAMETRANS_USER";
                    break;
                default:
                    throw std::invalid_argument(gppTr(
                        "NormalJsonTranslator.normalJsonInit",
                        "内部错误: 未知的 TransEngine")
                        .toStdString());
                }

                if (m_agentEnabled) {
                    m_agentSystemPrompt = readPromptString(systemPromptKey);
                    m_agentUserPrompt = readPromptString(userPromptKey);
                }
                else {
                    m_systemPrompt = readPromptString(systemPromptKey);
                    m_userPrompt = readPromptString(userPromptKey);
                }
            }
        }

        if (m_transEngine != TransEngine::NameTrans) {
            // 普通翻译路径需要分词和文本插件。
            const std::string tokenizerBackend = toml::find_or(configData, "common", "tokenize", "backend", "MeCab");
            if (tokenizerBackend == "MeCab") {
                const std::string mecabDictDir = toml::find_or(configData, "common", "tokenize", "mecabDictDir",
                    "BaseConfig/mecab/mecab-ipadic-utf8");
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "已配置 MeCab 分词器，首次使用时加载")
                    .toStdString());
                m_tokenizeSourceLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
            }
            else if (tokenizerBackend == "spaCy") {
                const std::string spaCyModelName = toml::find_or(configData, "common", "tokenize", "spaCyModelName",
                    "ja_core_news_lg");
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "已配置 spaCy 分词器，首次使用时加载")
                    .toStdString());
                m_tokenizeSourceLangFunc = getPythonNLPTokenizeFunc({ "click", "spacy" }, "tokenizer_spacy",
                    spaCyModelName, m_logger);
            }
            else if (tokenizerBackend == "Stanza") {
                const std::string stanzaLang = toml::find_or(configData, "common", "tokenize", "stanzaLang", "ja");
                m_logger->info(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "已配置 Stanza 分词器，首次使用时加载")
                    .toStdString());
                m_tokenizeSourceLangFunc = getPythonNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza",
                    stanzaLang, m_logger);
            }
            else {
                throw std::invalid_argument(gppTr(
                    "NormalJsonTranslator.normalJsonInit",
                    "无效的 tokenizerBackend: %1")
                    .arg(tokenizerBackend)
                    .toStdString());
            }

            const auto textPluginsOpt = toml::find<
                std::optional<std::vector<std::string>>
            >(configData, "plugins", "textPlugins");
            if (textPluginsOpt.has_value()) {
                registerPlugins(m_textPlugins, *textPluginsOpt, m_projectDir, m_otherCacheDir,
                    m_pythonManager, m_luaManager, m_logger, configData, true);
            }
        }
        else {
            m_gptDictionary = std::make_unique<GptDictionary>(
                m_projectDir, m_otherCacheDir, m_tokenizeSourceLangFunc,
                m_luaManager, m_pythonManager, m_logger
            );
            loadDictsFunc("gpt", m_gptDictionary);
        }

        // 需要翻译中后处理的模式会在这里补齐字典、插件和问题分析器。
        if (m_transEngine != TransEngine::ShowNormal && m_transEngine != TransEngine::GenDict && m_transEngine != TransEngine::NameTrans) {
            m_gptDictionary = std::make_unique<GptDictionary>(
                m_projectDir, m_otherCacheDir, m_tokenizeSourceLangFunc,
                m_luaManager, m_pythonManager, m_logger
            );
            loadDictsFunc("gpt", m_gptDictionary);
            m_postDictionary = std::make_unique<NormalDictionary>(m_projectDir, m_luaManager, m_pythonManager, m_logger);
            loadDictsFunc("post", m_postDictionary);

            const auto textPluginsOpt = toml::find<
                std::optional<std::vector<std::string>>
            >(configData, "plugins", "textPlugins");
            if (textPluginsOpt.has_value()) {
                registerPlugins(m_textPlugins, *textPluginsOpt, m_projectDir, m_otherCacheDir,
                    m_pythonManager, m_luaManager, m_logger, configData, false);
            }

            const std::string punctSet = toml::find_or(configData, "problemAnalyze", "punctSet",
                "（()）：:*[]{}<>『』「」“”;；'/\\");
            const std::string codePage = toml::find_or(configData, "problemAnalyze", "codePage", "gbk");
            const double langProbability = toml::find_or(configData, "problemAnalyze", "langProbability", 0.94);
            m_problemAnalyzer = std::make_unique<ProblemAnalyzer>(m_gptDictionary, m_targetLang, punctSet,
                codePage, langProbability, m_logger);
            struct ProblemRuleDefault
            {
                const char* key;
                bool enabled;
                const char* base;
                const char* check;
            };
            for (const ProblemRuleDefault& problemDefault : {
                ProblemRuleDefault{ "HighFrequency", false, "orig", "transview" },
                ProblemRuleDefault{ "PunctuationMismatch", true, "orig", "transview" },
                ProblemRuleDefault{ "LinebreakLost", true, "orig", "transview" },
                ProblemRuleDefault{ "LinebreakAdded", true, "orig", "transview" },
                ProblemRuleDefault{ "LongerThanSource", false, "orig", "transview" },
                ProblemRuleDefault{ "StrictlyLongerThanSource", false, "orig", "transview" },
                ProblemRuleDefault{ "DictionaryUnused", true, "preproc", "transview" },
                ProblemRuleDefault{ "JapaneseRemains", true, "orig", "transview" },
                ProblemRuleDefault{ "LatinIntroduced", false, "orig", "transview" },
                ProblemRuleDefault{ "HangulIntroduced", true, "preproc", "transview" },
                ProblemRuleDefault{ "TraditionalChineseIntroduced", true, "orig", "transview" },
                ProblemRuleDefault{ "NotTargetLanguage", false, "orig", "transview" },
                ProblemRuleDefault{ "InvalidCharacter", false, "orig", "transview" }
                })
            {
                m_problemAnalyzer->setProblemRule(
                    problemDefault.key,
                    toml::find_or(configData, "problemAnalyze", problemDefault.key, "enable", problemDefault.enabled),
                    toml::find_or(configData, "problemAnalyze", problemDefault.key, "base", std::string(problemDefault.base)),
                    toml::find_or(configData, "problemAnalyze", problemDefault.key, "check", std::string(problemDefault.check))
                );
            }

            const auto retranslKeysOpt =
                toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "retranslKeys");
            if (retranslKeysOpt.has_value()) {
                for (const auto& elem : retranslKeysOpt.value()) {
                    if (elem.is_string()) {
                        GppConditionPattern pattern;
                        pattern.conditionTarget = CachePart::Problems;
                        pattern.conditionReg.setPattern(elem.as_string()).setModifier(defaultRegCompileModifier).compile();
                        if (!pattern.conditionReg) {
                            throw std::runtime_error(gppTr(
                                "NormalJsonTranslator.normalJsonInit",
                                "retranslKeys 正则表达式 `%1` 编译失败")
                                .arg(elem.as_string())
                                .toStdString());
                        }
                        GPPCondition cond{ std::move(pattern) };
                        CheckSeCondNormalFunc checkFunc = [condr = std::move(cond)](const Sentence* se) -> bool
                        {
                            return checkGppCondition(condr, se);
                        };
                        m_retranslKeys.push_back(std::move(checkFunc));
                    }
                    else if (elem.is_array() || elem.is_table()) {
                        CheckSeCondNormalFunc checkFunc = getCheckSeCondFunc(elem, m_projectDir, m_pythonManager, m_luaManager, m_logger);
                        m_retranslKeys.push_back(std::move(checkFunc));
                    }
                    else {
                        throw std::invalid_argument(gppTr(
                            "NormalJsonTranslator.normalJsonInit",
                            "retranslKeys 的元素必须是字符串、表或表数组")
                            .toStdString());
                    }
                }
            }

            const auto skipProblemsOpt =
                toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "skipProblems");
            if (skipProblemsOpt.has_value()) {
                for (const auto& elem : skipProblemsOpt.value()) {
                    if (elem.is_string()) {
                        m_skipProblems.push_back({ jpc::Regex(elem.as_string(), defaultRegCompileModifier), std::nullopt });
                    }
                    else if (elem.is_array() && elem.size() > 0) {
                        if (!elem[0].is_string()) {
                            throw std::invalid_argument(gppTr(
                                "NormalJsonTranslator.normalJsonInit",
                                "skipProblems 的内联表数组第一个元素必须是字符串")
                                .toStdString());
                        }
                        jpc::Regex pattern(elem[0].as_string(), defaultRegCompileModifier);
                        if (elem.size() == 1) {
                            m_skipProblems.push_back({ std::move(pattern), std::nullopt });
                        }
                        else {
                            CheckSkipProblemCondFunc checkSkipFunc = getCheckSeCondFunc<const std::string&>
                                (elem, m_projectDir, m_pythonManager, m_luaManager, m_logger);
                            m_skipProblems.push_back({ std::move(pattern), std::move(checkSkipFunc) });
                        }
                    }
                    else {
                        throw std::invalid_argument(gppTr(
                            "NormalJsonTranslator.normalJsonInit",
                            "skipProblems 的元素必须是字符串或表数组")
                            .toStdString());
                    }
                }
            }

        }
    }
    catch (const toml::exception& e) {
        throw std::runtime_error(gppTr("NormalJsonTranslator.normalJsonInit", "项目配置文件解析失败: %1")
            .arg(e.what())
            .toStdString());
    }
}

void NormalJsonTranslator::recordSentenceDoneHelper(const fs::path& relInputPath, const Sentence& se, bool addToRuntimeTransSuccessStream) const
{
    m_controller->updateBar();
    if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
        m_controller->recordFileSentenceDone(wide2Ascii(relInputPath), !se.problems.empty());
        if (addToRuntimeTransSuccessStream && 
            !std::ranges::contains(se.problems, gppTr(
            "NormalJsonTranslator.postProcess",
            "翻译失败").toStdString()))
        {
            RuntimeTransSuccessEvent event;
            event.filename = wide2Ascii(relInputPath);
            event.index = se.index;
            if (se.nameType == NameType::Single && !se.nameTrans.empty()) {
                event.speakers.push_back(se.nameTrans);
            }
            else if (se.nameType == NameType::Multiple) {
                event.speakers = se.namesTrans;
            }
            event.sourcePreview = se.preproc;
            event.translationPreview = se.pretrans;
            event.translatedBy = se.translatedBy;
            m_controller->recordRuntimeTransSuccess(std::move(event));
        }
    }
}

void NormalJsonTranslator::preProcess(Sentence* se)
{
    se->preproc = se->orig;

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
        if (se->preproc.contains("<br>")) {
            se->linebreak = "<br>";
        }
        else if (se->preproc.contains("\\r\\n")) {
            se->linebreak = "\\r\\n";
        }
        else if (se->preproc.contains("\\n:")) {
            se->linebreak = "\\n:";
        }
        else if (se->preproc.contains("\\n")) {
            se->linebreak = "\\n";
        }
        else if (se->preproc.contains("\\r")) {
            se->linebreak = "\\r";
        }
        else if (se->preproc.contains("\r\n")) {
            se->linebreak = "\r\n";
        }
        else if (se->preproc.contains("\n")) {
            se->linebreak = "\n";
        }
        else if (se->preproc.contains("\r")) {
            se->linebreak = "\r";
        }
        else if (se->preproc.contains("[r][n]")) {
            se->linebreak = "[r][n]";
        }
        else if (se->preproc.contains("[n]")) {
            se->linebreak = "[n]";
        }
        else if (se->preproc.contains("[r]")) {
            se->linebreak = "[r]";
        }
    }
    else {
        se->linebreak = m_linebreakSymbol;
    }
    if (!se->linebreak.empty()) {
        replaceStrInplace(se->preproc, se->linebreak, "<br>");
    }
    replaceStrInplace(se->preproc, "\t", "<tab>");
    if (m_usePreDictInMsg) {
        se->preproc = m_preDictionary->doReplace(se, CachePart::Preproc);
    }

    for (auto& plugin : m_textPlugins) {
        plugin->preRun(se);
    }
}

void NormalJsonTranslator::postProcess(Sentence* se)
{
    se->nameTrans = se->name;
    se->namesTrans = se->names;
    se->transview = se->pretrans;

    if (se->transview.starts_with("(Failed to translate)")) {
            se->problems.push_back(gppTr("NormalJsonTranslator.postProcess", "翻译失败").toStdString());
        se->problemAnalyzeDisabled = true;
    }
    if (se->transview.starts_with("(GPPCProblem:")) {
        if (const size_t pos = se->transview.find(')'); pos != std::string::npos) {
            se->problems.push_back(std::format("GPPCProblem:{}", std::string_view(se->transview.data() + 13, pos - 13)));
            se->transview = se->transview.substr(pos + 1);
        }
    }
    else if (se->transview.contains("GPPCProblem")) {
        se->problems.push_back(gppTr("NormalJsonTranslator.postProcess", "错误的 GPPCProblem 格式")
            .toStdString());
    }

    for (auto& plugin : m_textPlugins) {
        plugin->postRun(se);
    }

    if (m_usePostDictInMsg) {
        se->transview = m_postDictionary->doReplace(se, CachePart::Transview);
    }
    replaceStrInplace(se->transview, "<tab>", "\t");
    if (!se->linebreak.empty()) {
        replaceStrInplace(se->transview, "<br>", se->linebreak);
    }

    auto replaceName = [&]()
        {
            if (m_useGptDictToReplaceName) {
                se->nameTrans = m_gptDictionary->doReplace(se, CachePart::NameTrans);
            }
            if (!se->nameTrans.empty()) {
                const auto it = m_nameMap.find(se->nameTrans);
                if (it != m_nameMap.end() && !it->second.empty()) {
                    se->nameTrans = it->second;
                }
            }
            if (m_usePostDictInName) {
                se->nameTrans = m_postDictionary->doReplace(se, CachePart::NameTrans);
            }
        };

    if (se->nameType != NameType::None) {
        if (se->nameType == NameType::Single) {
            replaceName();
        }
        else {
            for (auto& nameTrans : se->namesTrans) {
                se->nameTrans = std::move(nameTrans);
                replaceName();
                nameTrans = std::move(se->nameTrans);
            }
        }
    }

    for (auto& plugin : m_textPlugins) {
        plugin->dPostRun(se);
    }

    if (!se->problemAnalyzeDisabled) {
        m_problemAnalyzer->analyze(se);
    }

    std::erase_if(se->problems, [&](const std::string& problem)
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
                    return skipProblemCondition.second.value()(se, problem);
                });
        });
}
