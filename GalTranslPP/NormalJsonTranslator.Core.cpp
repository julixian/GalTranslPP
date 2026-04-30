module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Shlwapi.h>
#endif
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>
#include <proxy/proxy.h>

module NormalJsonTranslator;

import AgentSourceView;
import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NormalJsonTranslatorHelperTool;
import NLPTool;
import Tool;

namespace fs = std::filesystem;
namespace py = pybind11;

NormalJsonTranslator::~NormalJsonTranslator()
{
    m_logger->info("所有任务已完成！NormalJsonTranslator结束。");
}

NormalJsonTranslator::NormalJsonTranslator(
    const fs::path& projectDir,
    const std::shared_ptr<IController>& controller,
    const std::shared_ptr<spdlog::logger>& logger,
    std::optional<fs::path> inputDir,
    std::optional<fs::path> inputCacheDir,
    std::optional<fs::path> outputDir,
    std::optional<fs::path> outputCacheDir
) :
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
    m_agentTermConflictPath = m_agentRootDir / L"term_conflicts.json";
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

void NormalJsonTranslator::normalJsonInit()
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

        m_batchSize = toml::find_or(configData, "common", "numPerRequestTranslate", 16);
        m_threadsNum = toml::find_or(configData, "common", "threadsNum", 1);
        m_sortMethod = toml::find_or(configData, "common", "sortMethod", "name");
        m_targetLang = toml::find_or(configData, "common", "targetLang", "zh-cn");
        m_splitFile = toml::find_or(configData, "common", "splitFile", "No");
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
        m_agentMaxTurnsPerChunk = toml::find_or(configData, "agent", "maxTurnsPerChunk", 20);
        m_agentSoftContextChars = toml::find_or(configData, "agent", "softContextChars", 75000);
        m_agentHardContextChars = toml::find_or(configData, "agent", "hardContextChars", 100000);
        m_agentSearchResultLimit = toml::find_or(configData, "agent", "searchResultLimit", 80);
        m_agentRewriteMode = toml::find_or(configData, "agent", "rewriteMode", "queue_retranslate");

        if (m_agentEnabled) {
            if (m_transEngine == TransEngine::ForGalTsv || m_transEngine == TransEngine::ForNovelTsv) {
                if (m_agentMaxTurnsPerChunk <= 0 || m_agentSearchResultLimit <= 0 || m_agentSoftContextChars <= 0 || m_agentHardContextChars <= 0) {
                    throw std::invalid_argument("Agent 模式配置无效");
                }
                if (m_agentSoftContextChars > m_agentHardContextChars) {
                    std::swap(m_agentSoftContextChars, m_agentHardContextChars);
                }
                if (m_agentRewriteMode != "queue_retranslate" && m_agentRewriteMode != "mark_only") {
                    throw std::invalid_argument("Agent rewriteMode 当前仅支持 queue_retranslate / mark_only");
                }
            }
            else if (m_transEngine == TransEngine::GenDict) {
                if (m_agentMaxTurnsPerChunk <= 0 || m_agentSearchResultLimit <= 0) {
                    throw std::invalid_argument("GenDict Review Agent 配置无效");
                }
            }
            else {
                throw std::invalid_argument("Agent 模式当前仅支持 ForGalTsv / ForNovelTsv / GenDict");
            }
        }

        m_usePreDictInName = toml::find_or(configData, "dictionary", "usePreDictInName", false);
        m_usePostDictInName = toml::find_or(configData, "dictionary", "usePostDictInName", false);
        m_usePreDictInMsg = toml::find_or(configData, "dictionary", "usePreDictInMsg", true);
        m_usePostDictInMsg = toml::find_or(configData, "dictionary", "usePostDictInMsg", true);
        m_useGptDictToReplaceName = toml::find_or(configData, "dictionary", "useGPTDictInName", false);
        const std::string defaultDictFolder = toml::find_or(configData, "dictionary", "defaultDictFolder", "BaseConfig/Dict");
        const fs::path defaultDictFolderPath = ascii2Wide(defaultDictFolder);

        const auto registerAgentDictionaryPath = [&](const fs::path& dictPath)
            {
                const fs::path normalized = fs::canonical(dictPath);
                if (std::ranges::find(m_agentDictionaryPaths, normalized) == m_agentDictionaryPaths.end()) {
                    m_agentDictionaryPaths.push_back(normalized);
                }
            };

        const auto resolveDictPaths = [&](const std::string& dictType)
            {
                std::vector<fs::path> dictPaths;
                const auto dictFileNames = toml::find<
                    std::optional<std::vector<std::string>>
                >(configData, "dictionary", dictType + "Dict");
                if (!dictFileNames) {
                    return dictPaths;
                }
                for (const auto& dictFileName : *dictFileNames) {
                    fs::path dictPath = m_projectDir / ascii2Wide(dictFileName);
                    if (fs::exists(dictPath)) {
                        dictPaths.push_back(dictPath);
                        continue;
                    }

                    dictPath = defaultDictFolderPath / ascii2Wide(dictType) / ascii2Wide(dictFileName);
                    if (fs::exists(dictPath)) {
                        dictPaths.push_back(dictPath);
                    }
                }
                return dictPaths;
            };

        auto loadDictsFunc = [&]<typename DictionaryType>(const std::string& dictType, DictionaryType& dict)
            {
                for (const fs::path& dictPath : resolveDictPaths(dictType)) {
                    dict->loadFromFile(dictPath);
                    if (m_agentEnabled && dictType == "gpt") {
                        registerAgentDictionaryPath(dictPath);
                    }
                }
                dict->sort();
            };

        if (m_agentEnabled) {
            static const std::array<fs::path, 6> agentProjectInfoCandidates = {
                L"脚本说明.md",
                L"剧情说明.md",
                L"设定补充.md",
                L"script_info.md",
                L"story_info.md",
                L"project_note.md"
            };
            for (const fs::path& candidate : agentProjectInfoCandidates) {
                const fs::path fullPath = m_projectDir / candidate;
                if (fs::exists(fullPath) && fs::is_regular_file(fullPath)) {
                    m_agentProjectInfoPath = fs::canonical(fullPath);
                    break;
                }
            }
        }
        m_preDictionary = std::make_unique<NormalDictionary>(m_projectDir, m_luaManager, m_pythonManager, m_logger);
        loadDictsFunc("pre", m_preDictionary);

        if (m_transEngine == TransEngine::DumpName) {
            return;
        }

        // 这些模式需要初始化 API 池和提示词。
        if (m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
            m_apiStrategy = toml::find_or(configData, "backendSpecific", "OpenAI-Compatible", "apiStrategy", "random");
            if (m_apiStrategy != "random" && m_apiStrategy != "fallback") {
                throw std::invalid_argument("apiStrategy must be random or fallback");
            }
            int apiTimeOutSecond = toml::find_or(configData, "backendSpecific", "OpenAI-Compatible", "apiTimeout", 300);
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
                if (api.stream && m_agentEnabled) {
                    throw std::invalid_argument("Agent 模式不支持流式 api 响应");
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
                throw std::invalid_argument("找不到可用的 API key ");
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

            if (m_transEngine == TransEngine::GenDict) {
                m_systemPrompt = readPromptString("GENDICT_SYSTEM");
                m_userPrompt = readPromptString("GENDICT_PROMPT");
                if (m_agentEnabled) {
                    m_genDictReviewSystemPrompt = readPromptString("GENDICT_REVIEW_SYSTEM");
                    m_genDictReviewUserPrompt = readPromptString("GENDICT_REVIEW_PROMPT");
                }
            }
            else {
                std::string systemPromptKey;
                std::string userPromptKey;

                switch (m_transEngine)
                {
                case TransEngine::ForGalJson:
                    systemPromptKey = "FORGALJSON_SYSTEM";
                    userPromptKey = "FORGALJSON_TRANS_PROMPT_EN";
                    break;
                case TransEngine::ForGalTsv:
                    systemPromptKey = m_agentEnabled ? "FORGALTSV_AGENT_SYSTEM" : "FORGALTSV_SYSTEM";
                    userPromptKey = m_agentEnabled ? "FORGALTSV_AGENT_PROMPT_EN" : "FORGALTSV_TRANS_PROMPT_EN";
                    break;
                case TransEngine::ForNovelTsv:
                    systemPromptKey = m_agentEnabled ? "FORNOVELTSV_AGENT_SYSTEM" : "FORNOVELTSV_SYSTEM";
                    userPromptKey = m_agentEnabled ? "FORNOVELTSV_AGENT_PROMPT_EN" : "FORNOVELTSV_TRANS_PROMPT_EN";
                    break;
                case TransEngine::DeepseekJson:
                    systemPromptKey = "DEEPSEEKJSON_SYSTEM_PROMPT";
                    userPromptKey = "DEEPSEEKJSON_TRANS_PROMPT";
                    break;
                case TransEngine::Sakura:
                    systemPromptKey = "SAKURA_SYSTEM_PROMPT";
                    userPromptKey = "SAKURA_TRANS_PROMPT";
                    break;
                case TransEngine::NameTrans:
                    systemPromptKey = "NAMETRANS_SYSTEM";
                    userPromptKey = "NAMETRANS_PROMPT";
                    break;
                default:
                    throw std::invalid_argument("未知的 TransEngine");
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

bool NormalJsonTranslator::shouldReportRuntimeWorkbench() const
{
    return m_transEngine != TransEngine::Rebuild;
}

void NormalJsonTranslator::recordSentenceDone(const fs::path& relInputPath, const Sentence& se, bool addToSuccessStream) const
{
    m_controller->updateBar();
    if (!shouldReportRuntimeWorkbench()) {
        return;
    }
    m_controller->recordFileSentenceDone(wide2Ascii(relInputPath), !se.problems.empty());
    if (addToSuccessStream && !std::ranges::contains(se.problems, "翻译失败")) {
        RuntimeSuccessEvent event;
        event.filename = wide2Ascii(relInputPath);
        event.index = se.index;
        if (se.nameType == NameType::Single && !se.name_preview.empty()) {
            event.speakers.push_back(se.name_preview);
        }
        else if (se.nameType == NameType::Multiple) {
            event.speakers = se.names_preview;
        }
        event.sourcePreview = se.pre_processed_text;
        event.translationPreview = se.pre_translated_text;
        event.translatedBy = se.translated_by;
        m_controller->recordRuntimeSuccess(std::move(event));
    }
}

void NormalJsonTranslator::recordRuntimeError(const std::string& kind, const std::string& message, const fs::path& relInputPath,
    const std::string& indexRange, int retryCount, const std::string& model, double sleepSeconds, const std::string& level) const
{
    if (!shouldReportRuntimeWorkbench()) {
        return;
    }
    RuntimeErrorEvent event;
    event.kind = kind;
    event.level = level;
    event.message = message;
    event.filename = relInputPath.empty() ? std::string{} : wide2Ascii(relInputPath);
    event.indexRange = indexRange;
    event.retryCount = retryCount;
    event.model = model;
    event.sleepSeconds = sleepSeconds;
    m_controller->recordRuntimeError(std::move(event));
}

void NormalJsonTranslator::preProcess(Sentence* se)
{
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

void NormalJsonTranslator::postProcess(Sentence* se)
{
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
            for (auto& namePreview : se->names_preview) {
                se->name_preview = std::move(namePreview);
                replaceName();
                namePreview = std::move(se->name_preview);
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
                    return [&]()
                        {
                            problem = "Current problem:" + problem;
                            const bool result = skipProblemCondition.second.value()(se);
                            problem = problem.substr(16);
                            return result;
                        }();
                });
        });
}
