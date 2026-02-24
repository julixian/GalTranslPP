module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#ifdef _WIN32
#include <Windows.h>
#include <Shlwapi.h>
#endif
#include <spdlog/spdlog.h>
#include <toml.hpp>
#include <ctpl_stl.h>
#include <sol/sol.hpp>

module NormalJsonTranslator;

import ConditionTool;
import DictionaryGenerator;
import NameTranslator;
import NJ_ImplTool;
import NLPTool;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace py = pybind11;
namespace fs = std::filesystem;

NormalJsonTranslator::~NormalJsonTranslator() 
{
    m_logger->info("所有任务已完成！NormalJsonTranslator结束。");
}

NormalJsonTranslator::NormalJsonTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
                                           std::optional<fs::path> inputDir, std::optional<fs::path> inputCacheDir,
                                           std::optional<fs::path> outputDir, std::optional<fs::path> outputCacheDir) 
    :
    m_projectDir(projectDir), m_controller(controller), m_logger(logger), 
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
    try {
        if (fs::exists(m_backgroundTextCachePath)) {
            std::ifstream ifs(m_backgroundTextCachePath);
            m_backgroundTextCacheMap = json::parse(ifs).get<decltype(m_backgroundTextCacheMap)>();
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
        bool needReboot = false;
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

        m_batchSize = toml::find_or(configData, "common", "numPerRequestTranslate", 10);
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
                        dict->loadFromFile(dictPath, needReboot);
                    }
                    else {
                        dictPath = defaultDictFolderPath / ascii2Wide(dictType) / ascii2Wide(dictFileName);
                        if (fs::exists(dictPath)) {
                            dict->loadFromFile(dictPath, needReboot);
                        }
                    }
                }
                dict->sort();
            };
        m_preDictionary = std::make_unique<NormalDictionary>(m_projectDir, m_luaManager, m_pythonManager, m_logger);
        loadDictsFunc("pre", m_preDictionary);

        if (m_transEngine != TransEngine::DumpName) {
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

                fs::path promptPath = m_projectDir / L"Prompt.toml";
                if (!fs::exists(promptPath)) {
                    promptPath = defaultPromptPath;
                    if (!fs::exists(promptPath)) {
                        throw std::runtime_error("找不到 Prompt.toml 文件");
                    }
                }

                const auto promptData = toml::uparse(promptPath);

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

                if (promptData.contains(systemKey) && promptData.at(systemKey).is_string()) {
                    m_systemPrompt = promptData.at(systemKey).as_string();
                }
                else {
                    throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", systemKey));
                }
                if (promptData.contains(userKey) && promptData.at(userKey).is_string()) {
                    m_userPrompt = promptData.at(userKey).as_string();
                }
                else {
                    throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", userKey));
                }
            }


            if (m_transEngine != TransEngine::NameTrans) {
                // 需要翻译预处理
                const std::string tokenizerBackend = toml::find_or(configData, "common", "tokenizerBackend", "MeCab");
                if (tokenizerBackend == "MeCab") {
                    const std::string mecabDictDir = toml::find_or(configData, "common", "mecabDictDir", "BaseConfig/mecabDict/mecab-ipadic-utf8");
                    m_logger->info("正在检查 MeCab 环境...");
                    m_tokenizeSourceLangFunc = getMeCabTokenizeFunc(mecabDictDir, m_logger);
                    m_logger->info("MeCab 环境检查完毕。");
                }
                else if (tokenizerBackend == "spaCy") {
                    const std::string spaCyModelName = toml::find_or(configData, "common", "spaCyModelName", "ja_core_news_lg");
                    m_logger->info("正在检查 spaCy 环境...");
                    m_tokenizeSourceLangFunc = getNLPTokenizeFunc({ "spacy" }, "tokenizer_spacy", spaCyModelName, m_logger, needReboot);
                    m_logger->info("spaCy 环境检查完毕。");
                }
                else if (tokenizerBackend == "Stanza") {
                    const std::string stanzaLang = toml::find_or(configData, "common", "stanzaLang", "ja");
                    m_logger->info("正在检查 Stanza 环境...");
                    m_tokenizeSourceLangFunc = getNLPTokenizeFunc({ "stanza" }, "tokenizer_stanza", stanzaLang, m_logger, needReboot);
                    m_logger->info("Stanza 环境检查完毕。");
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

                const auto retranslKeys = toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "retranslKeys");
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
                            CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, m_projectDir, m_pythonManager, m_luaManager, m_logger, needReboot);
                            m_retranslKeys.push_back(std::move(checkFunc));
                        }
                        else {
                            throw std::invalid_argument("retranslKeys 的元素必须是字符串、表或表数组");
                        }
                    }
                }

                const auto skipProblems = toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "skipProblems");
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
                                CheckSeCondFunc checkFunc = getCheckSeCondFunc(elem, m_projectDir, m_pythonManager, m_luaManager, m_logger, needReboot);
                                m_skipProblems.push_back({ std::move(pattern), std::move(checkFunc) });
                            }
                        }
                        else {
                            throw std::invalid_argument("skipProblems 的元素必须是字符串或表数组");
                        }
                    }
                }

                const auto overwriteCompareObj = toml::find<std::optional<toml::array>>(configData, "problemAnalyze", "overwriteCompareObj");
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

        needReboot = needReboot || std::ranges::any_of(m_textPlugins, [](const auto& plugin) { return plugin->needReboot(); });
        if (needReboot) {
            throw std::runtime_error("需要重启程序以应用新安装的 NLP 模型");
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
        if (size_t pos = se->translated_preview.find(')'); pos != std::string::npos) {
            se->problems.push_back(std::format("GPPCProblem:{}", std::string_view(se->translated_preview.data() + 13, pos - 13)));
            se->translated_preview = se->translated_preview.substr(pos + 1);
        }
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

    auto replaceName = [&]() -> std::string
        {
            if (m_useGptDictToReplaceName) {
                se->name_preview = m_gptDictionary->doReplace(se, CachePart::NamePreview);
            }
            if (!se->name_preview.empty()) {
                auto it = m_nameMap.find(se->name_preview);
                if (it != m_nameMap.end() && !it->second.empty()) {
                    se->name_preview = it->second;
                }
            }
            if (m_usePostDictInName) {
                se->name_preview = m_postDictionary->doReplace(se, CachePart::NamePreview);
            }
            return se->name_preview;
        };

    if (se->nameType != NameType::None) {
        if (se->nameType == NameType::Single) {
            replaceName();
        }
        else {
            for (auto& name_preivew : se->names_preview) {
                se->name_preview = std::move(name_preivew);
                name_preivew = replaceName();
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
                    bool problemMatch = checkString(skipProblemCondition.first, problem);
                    if (!problemMatch) {
                        return false;
                    }
                    if (!skipProblemCondition.second.has_value()) {
                        return true;
                    }
                    else {
                        auto checkFunc = [&]() -> bool
                            {
                                problem = "Current problem:" + problem;
                                const bool result = skipProblemCondition.second.value()(se);
                                problem = problem.substr(16);
                                return result;
                            };
                        return checkFunc();
                    }
                });
        });
}


bool NormalJsonTranslator::translateBatchWithRetry(const fs::path& relInputPath, std::vector<Sentence*>& batch, std::string& backgroundText, int threadId) {

    for (Sentence* pSentence : batch) {
        if (pSentence->pre_processed_text.empty()) {
            pSentence->complete = true;
            m_completedSentences++;
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

        std::vector<Sentence*> batchToTransThisRound;
        for (Sentence* pSentence : batch | std::views::filter([](const Sentence* pSentence) { return !pSentence->complete; })) {
            batchToTransThisRound.push_back(pSentence);
        }
        if (batchToTransThisRound.empty()) {
            return true;
        }

        if (m_smartRetry && retryCount == 2 && batchToTransThisRound.size() > 1) {
            m_logger->warn("[线程 {}] [文件 {}] 开始拆分批次进行重试...", threadId, wide2Ascii(relInputPath));

            size_t mid = batchToTransThisRound.size() / 2;
            std::vector<Sentence*> firstHalf(batchToTransThisRound.begin(), batchToTransThisRound.begin() + mid);
            std::vector<Sentence*> secondHalf(batchToTransThisRound.begin() + mid, batchToTransThisRound.end());

            bool firstOk = translateBatchWithRetry(relInputPath, firstHalf, backgroundText, threadId);
            bool secondOk = translateBatchWithRetry(relInputPath, secondHalf, backgroundText, threadId);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] [文件 {}] 清空上下文后再次尝试...", threadId, wide2Ascii(relInputPath));
            contextHistory.clear();
            backgroundText.clear();
        }


        std::string inputProblems;
        for (const auto& problem : batchToTransThisRound
            | std::views::transform([](const Sentence* pSentence) { return pSentence->problems; })
            | std::views::join) {
            if (!inputProblems.contains(problem)) {
                inputProblems += problem + "\n";
            }
        }

        std::string inputBlock;
        std::map<int, Sentence*> id2SentenceMap; // 用于 TSV/JSON 
        fillBlockAndMap(batchToTransThisRound, id2SentenceMap, inputBlock, m_transEngine);

        std::string logBlock;
        if (!inputProblems.empty()) {
            logBlock += "\nProblems:\n" + inputProblems;
        }
        if (m_logger->should_log(spdlog::level::debug) && !backgroundText.empty()) {
            logBlock += "\nBackground:\n" + backgroundText + "\n";
        }
        if (m_logger->should_log(spdlog::level::trace) && !contextHistory.empty()) {
            logBlock += "\nContext:\n" + contextHistory;
        }
        if (!glossary.empty()) {
            logBlock += "\nDict:\n" + glossary;
        }
        logBlock += "\ninputBlock:\n" + inputBlock;
        m_logger->info("[线程 {}] [文件 {}] 开始翻译:\n{}", threadId, wide2Ascii(relInputPath), logBlock);
        std::string promptReq = m_userPrompt;
        replaceStrInplace(promptReq, "[Problem Description]", inputProblems.empty() ? "None" : inputBlock);
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

        std::optional<TranslationApi> apiOpt = m_apiStrategy == "random" ? m_apiPool->getApi() : m_apiPool->getFirstApi();
        if (!apiOpt.has_value()) {
            throw std::runtime_error("没有可用的API Key了");
        }
        const TranslationApi& currentApi = apiOpt.value();

        json payload = { {"messages", messages} };

        ApiResponse response = performApiRequest(payload, currentApi, m_onPerformApi, m_controller, m_logger, threadId, m_apiTimeOutMs);

        /*bool checkResponse(const ApiResponse& response, std::unique_ptr<APIPool>& m_apiPool, const TranslationApi& currentAPI,
            const std::filesystem::path& relInputPath, const std::string& m_apiStrategy, std::shared_ptr<spdlog::logger>& m_logger,
            int& retryCount, int threadId, bool m_checkQuota);*/
        if (!checkResponse(
            response, m_apiPool, currentApi, relInputPath, m_apiStrategy, m_logger, retryCount, threadId, m_checkQuota
        )) {
            continue;
        }
        else {
            m_logger->trace("[线程 {}] [文件 {}] 成功响应，响应内容:\n{}", threadId, wide2Ascii(relInputPath), response.content);
        }

        // --- 如果请求成功，则继续解析 ---
        int parsedCount = 0;

        //void parseContent(std::string& content, std::vector<Sentence*>& batchToTransThisRound, std::map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
        //    std::shared_ptr<IController>& controller, std::string& backgroudText, std::atomic<int>& completedSentences, int& parsedCount, TransEngine transEngine, bool showBackgroundText);
        parseContent(response.content, batchToTransThisRound, id2SentenceMap, currentApi.modelName,
            m_controller, backgroundText, m_completedSentences, parsedCount, m_transEngine, m_logger->should_log(spdlog::level::debug));

        if (parsedCount != batchToTransThisRound.size()) {
            ++retryCount;
            if (!m_controller->shouldStop()) {
                m_logger->warn("[线程 {}] [文件 {}] 解析失败或不完整 ({} / {}), 进行第 {} 次重试...", threadId, wide2Ascii(relInputPath), parsedCount, batchToTransThisRound.size(), retryCount);
            }
            continue;
        }
        else {
            m_logger->info("[线程 {}] [文件 {}] 成功解析 {} 句，解析结果: \n{}", threadId, wide2Ascii(relInputPath), parsedCount, response.content);
        }

        return true;
    }

    size_t failedCount = 0;
    for (Sentence* pSentence : batch | std::views::filter([](const Sentence* pSentence) { return !pSentence->complete; })) {
        ++failedCount;
        pSentence->pre_translated_text = "(Failed to translate)" + pSentence->pre_processed_text;
        pSentence->complete = true;
        m_completedSentences++;
        m_controller->updateBar(); // 失败
    }
    m_logger->error("[线程 {}] [文件 {}] 批次翻译在 {} 次重试后彻底失败，共翻译 {} / {} 句。",
        threadId, wide2Ascii(relInputPath), retryCount, batch.size() - failedCount, batch.size());
    return false;
}


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

    // 解析输入文件
    try {
        ifs.open(inputPath);
        jSentences = ordered_json::parse(ifs);
        ifs.close();
        for (const auto& [index, item] : jSentences | std::views::enumerate) {
            Sentence se;
            se.index = (int)index;
            if (item.contains("name")) {
                se.nameType = NameType::Single;
                se.name = item["name"].get<std::string>();
            }
            else if (item.contains("names")) {
                se.nameType = NameType::Multiple;
                se.names = item["names"].get<std::vector<std::string>>();
            }
            se.original_text = item["message"].get<std::string>();
            sentences.push_back(std::move(se));
        }
        for (size_t i = 0; i < sentences.size(); ++i) {
            if (i > 0) sentences[i].prev = &sentences[i - 1];
            if (i + 1 < sentences.size()) sentences[i].next = &sentences[i + 1];
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
            m_completedSentences++;
            m_controller->updateBar(); // ShowNormal
        }
        createParent(showNormalPath);
        std::ofstream ofs(showNormalPath);
        ofs << showNormalJson.dump(2);
        ofs.close();
        return;
    }
    // ShowNormal结束

    std::vector<Sentence*> toTranslate;

    // 读缓存逻辑
    {
        std::unordered_map<std::string, json> cacheMap;

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
                        ifs.open(potentialCachePath);
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
                    if (PathMatchSpecW(entry.path().filename().wstring().c_str(), cacheSpec.c_str())) {
                        if (m_needsCombining) {
                            int diff = calculateCachePartIndexDiff(relInputPath.wstring(), entry.path().wstring());
                            if (std::abs(diff) > m_cacheSearchDistance) {
                                continue;
                            }
                        }
                        if (!std::ranges::contains(cachePaths, entry.path())) {
                            cachePaths.push_back(entry.path());
                        }
                    }
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
                std::optional<fs::path> additionalCachePath = std::nullopt;
                if (auto it = m_splitFilePartsToJson.find(relInputPath); it != m_splitFilePartsToJson.end() && fs::exists(m_transCacheDir / it->second)) {
                    additionalCachePath = m_transCacheDir / it->second;
                }
                // 这个逻辑还挺耗时的
                size_t pos = relInputPath.filename().wstring().rfind(L"_part_");
                std::wstring orgStem = relInputPath.filename().wstring().substr(0, pos);
                std::wstring cacheSpec = orgStem + L"_part_*.json";
                // 分割优先读分割缓存
                readAllPotentialPartFileCache(cacheSpec, m_transCacheDir / relInputPath.parent_path(), additionalCachePath);
            }
            else {
                std::wstring cacheSpec = relInputPath.stem().wstring() + L"_part_*.json";
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
                        ifs.open(cp);
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
                m_completedSentences++;
                m_controller->updateBar(); // 跳过已完成的句子
                postProcess(&se);
                continue;
            }
            const std::string key = generateCacheKey(&se);
            auto it = cacheMap.find(key);
            if (it == cacheMap.end()) {
                toTranslate.push_back(&se);
                continue;
            }
            const auto& item = it->second;
            // 命中缓存了就把 problems 带上
            if (item.contains("problems")) {
                se.problems = item["problems"].get<std::vector<std::string>>();
            }
            if (m_transEngine != TransEngine::Rebuild && hasRetranslKey(m_retranslKeys, item, &se)) {
                toTranslate.push_back(&se);
                continue;
            }
            else {
                se.pre_translated_text = item.value("pre_translated_text", "");
                se.translated_by = item.value("translated_by", "");
                se.complete = true;
                m_completedSentences++;
                m_controller->updateBar(); // 命中缓存
                postProcess(&se);
            }
        }

        m_logger->info("[线程 {}] [文件 {}] 共 {} 句，命中缓存/跳过 {} 句，需翻译 {} 句。", threadId, wide2Ascii(relInputPath),
            sentences.size(), sentences.size() - toTranslate.size(), toTranslate.size());

        if (m_transEngine == TransEngine::Rebuild && !toTranslate.empty()) {
            std::string notFoundSentences;
            for (const auto& se : toTranslate) {
                notFoundSentences += se->original_text + "\n";
            }
            m_logger->critical("[线程 {}] [文件 {}] 有 {} 句未命中缓存，这些句子是: {}", threadId, wide2Ascii(relInputPath), toTranslate.size(), notFoundSentences);
            std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
            saveCache(sentences, cachePath);
            return;
        }
    }
    // 读缓存逻辑结束

    // 翻译逻辑
    if (m_transEngine != TransEngine::Rebuild && !toTranslate.empty()) {
        std::unique_ptr<py::gil_scoped_release> release;
        if (m_pythonTranslator) {
            release = std::make_unique<py::gil_scoped_release>();
        }
        int batchCount = 0;
        std::string backgroundText;
        std::string filePathWithHash = std::format("{}{:08X}", wide2Ascii(relInputPath), calculateFileCRC64(inputPath));
        {
            std::lock_guard<std::mutex> lock(m_backgroundTextCacheMapMutex);
            if (auto it = m_backgroundTextCacheMap.find(filePathWithHash); it != m_backgroundTextCacheMap.end()) {
                backgroundText = it->second;
            }
        }
        Sentence* pLastSentence = nullptr;
        for (size_t i = 0; i < toTranslate.size(); i += m_batchSize) {
            std::vector<Sentence*> batch(toTranslate.begin() + i, toTranslate.begin() + std::min(i + m_batchSize, toTranslate.size()));
            if (i != 0 && !backgroundText.empty() && pLastSentence) {
                if (batch.front()->index - pLastSentence->index > m_batchSize) {
                    backgroundText.clear();
                }
            }
            if (m_controller->shouldStop()) {
                if (!backgroundText.empty()) {
                    std::lock_guard<std::mutex> lock(m_backgroundTextCacheMapMutex);
                    m_backgroundTextCacheMap[filePathWithHash] = backgroundText;
                }
                m_logger->debug("[线程 {}] [文件 {}] 已停止翻译", threadId, wide2Ascii(relInputPath));
                return;
            }
            translateBatchWithRetry(relInputPath, batch, backgroundText, threadId);
            pLastSentence = batch.back();
            for (Sentence* se : batch) {
                postProcess(se);
            }
            if (++batchCount % m_saveCacheInterval == 0) {
                m_logger->debug("[线程 {}] [文件 {}] 达到保存间隔，正在更新缓存文件...", threadId, wide2Ascii(relInputPath));
                std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
                saveCache(sentences, cachePath);
            }
        }
        {
            std::lock_guard<std::mutex> lock(m_backgroundTextCacheMapMutex);
            m_backgroundTextCacheMap.erase(filePathWithHash);
        }
    }
    // 翻译逻辑结束

    // 最终保存缓存逻辑
    {
        std::lock_guard<std::shared_mutex> lock(m_transCacheMutex);
        m_logger->debug("[线程 {}] [文件 {}] 翻译完成，正在进行最终保存...", threadId, wide2Ascii(relInputPath));
        saveCache(sentences, cachePath);

        std::string relInputPathStr = wide2Ascii(relInputPath);
        for (const auto& se : sentences) {
            if (se.problems.empty()) {
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

    std::ofstream ofs(outputPath);
    ofs << jSentences.dump(2);
    ofs.close();

    m_logger->info("[线程 {}] [文件 {}] 处理完成。", threadId, wide2Ascii(relInputPath));

    if (m_needsCombining) {
        const fs::path& originalRelFilePath = m_splitFilePartsToJson[relInputPath];
        auto& splitFileParts = m_jsonToSplitFileParts[originalRelFilePath];
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
        std::unique_lock<std::mutex> lock(m_outputMutex);
        if (m_onFileProcessed) {
            if (m_pythonTranslator) {
                lock.unlock(); // 非常神奇,总之如果 m_onFileProcessed 是 python 侧赋值的闭包的话，这里的不 unlock 就会死锁
            }
            m_onFileProcessed(originalRelFilePath);
        }
        m_logger->debug("[线程 {}] [文件 {}] 合并处理完成。", threadId, wide2Ascii(originalRelFilePath));
    }
    else {
        std::unique_lock<std::mutex> lock(m_outputMutex);
        if (m_onFileProcessed) {
            if (m_pythonTranslator) {
                lock.unlock();
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
        std::map<std::string, int> nameTableMap;
        Sentence se;

        auto insertNameTable = [&](const std::string& name)
            {
                if (!name.empty()) {
                    nameTableMap[name]++;
                }
            };

        // 收集人名和json相对路径，检查msg字段是否存在
        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }
            const fs::path relInputPath = fs::relative(entry.path(), m_inputDir);
            relJsonPaths.push_back(relInputPath);
            try {
                ifs.open(entry.path());
                json data = json::parse(ifs);
                ifs.close();

                for (const auto& [index, item] : data | std::views::enumerate) {
                    m_totalSentences++;
                    if (item.contains("name")) {
                        se.name = item["name"].get<std::string>();
                        if (m_usePreDictInName) {
                            se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                        }
                        insertNameTable(se.name);
                    }
                    else if (item.contains("names")) {
                        se.names = item["names"].get<std::vector<std::string>>();
                        for (const auto& name : item["names"]) {
                            se.name = name.get<std::string>();
                            if (m_usePreDictInName) {
                                se.name = m_preDictionary->doReplace(&se, CachePart::Name);
                            }
                            insertNameTable(se.name);
                        }
                        se.names.clear();
                    }
                    if (!item.contains("message")) {
                        throw std::runtime_error(std::format("[文件 {}] 第 {} 个对象缺少 message 字段。", wide2Ascii(relInputPath), index));
                    }
                }
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

        std::vector<std::pair<std::string, int>> nameTableKeys;
        for (const auto& nameCountPair : nameTableMap) {
            nameTableKeys.push_back(nameCountPair);
        }
        std::ranges::sort(nameTableKeys, [&](const auto& a, const auto& b)
            {
                if (a.second == b.second) {
                    return a.first.length() > b.first.length();
                }
                return a.second > b.second;
            });

        toml::ordered_value newNameTable = toml::ordered_table{};
        newNameTable.comments().push_back("'原名' = [ '译名', 出现次数 ]");
        for (const std::string& key : nameTableKeys | std::views::keys) {
            try {
                newNameTable[key] = toml::array{ toml::find_or(orgNameTable, key, 0, ""), nameTableMap[key] };
            }
            catch (...) {
                newNameTable[key] = toml::array{ "", nameTableMap[key] };
            }
        }
        ofs.open(nameTablePath);
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
            preProcessFunc, m_onPerformApi, m_onDictProcessed,
            m_systemPrompt, m_userPrompt, m_apiStrategy, m_targetLang,
            m_maxRetries, m_threadsNum, m_apiTimeOutMs, m_checkQuota);
        const fs::path outputFilePath = m_projectDir / L"项目GPT字典-生成.toml";
        std::vector<fs::path> inputPaths = std::move(relJsonPaths);
        for (auto& inputPath : inputPaths) {
            inputPath = m_inputDir / inputPath;
        }
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
                        ifs.open(m_inputDir / relJsonPath);
                        ordered_json data = ordered_json::parse(ifs);
                        ifs.close();
                        const std::vector<ordered_json> parts = splitImplFunc(data, m_splitFileNum);
                        const std::wstring relStem = relJsonPath.parent_path() / relJsonPath.stem();
                        for (size_t i = 0; i < parts.size(); ++i) {
                            const fs::path relPartPath = std::format(L"{}_part_{}{}", relStem, i, relJsonPath.extension().wstring());
                            m_splitFilePartsToJson[relPartPath] = relJsonPath;
                            m_jsonToSplitFileParts[relJsonPath].insert({ relPartPath, false });
                            const fs::path partPath = m_inputCacheDir / relPartPath;
                            createParent(partPath);
                            ofs.open(partPath);
                            ofs << parts[i].dump(2);
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
    std::vector<fs::path> relFilePaths;
    if (!m_needsCombining) {
        relFilePaths = std::move(relJsonPaths);
    }
    else {
        for (const auto& relPartPath : m_splitFilePartsToJson | std::views::keys) {
            relFilePaths.push_back(relPartPath);
        }
    }
    if (relFilePaths.empty()) {
        throw std::runtime_error("未找到任何待翻译文件。");
    }

    if (m_sortMethod == "size") {
        std::ranges::sort(relFilePaths, [&](const fs::path& a, const fs::path& b)
            {
                return m_needsCombining ? (fs::file_size(m_inputCacheDir / a) > fs::file_size(m_inputCacheDir / b)) :
                    (fs::file_size(m_inputDir / a) > fs::file_size(m_inputDir / b));
            });
    }
    else if (m_sortMethod == "name") {
        std::ranges::sort(relFilePaths);
    }
    else {
        throw std::invalid_argument(std::format("未知的排序模式: {}", m_sortMethod));
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
        ofs.open(m_projectDir / L"翻译问题概览.toml");
        ofs << toml::format("problemOverview", m_problemOverview);
        ofs.close();
        ofs.open(m_projectDir / L"翻译问题概览.json");
        ofs << toml2Json(m_problemOverview).dump(2);
        ofs.close();
        m_logger->debug("已生成 翻译问题概览.json 和 翻译问题概览.toml 文件");

        std::map<std::string, std::set<std::string>> problemMap;
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
                fileCount++;
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
            json j = m_backgroundTextCacheMap;
            createParent(m_backgroundTextCachePath);
            std::ofstream ofs(m_backgroundTextCachePath);
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
    try {
        if (Py_IsInitialized()) {
            PythonMainInterpreterManager::getInstance().releaseNLPResources(m_logger);
        }
    }
    catch (const std::exception& e) {
        m_logger->warn("执行 NLP GC 失败: {}", e.what());
    }
}

void NormalJsonTranslator::process(std::vector<fs::path> relFilePaths) {
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
