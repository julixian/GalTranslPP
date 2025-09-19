module;

#include <Windows.h>
#include <Shlwapi.h>
#include <boost/regex.hpp>
#include <spdlog/spdlog.h>
#pragma comment(lib, "Shlwapi.lib")

export module NormalJsonTranslator;

import <nlohmann/json.hpp>;
import <toml++/toml.hpp>;
import <ctpl_stl.h>;
import Tool;
import APIPool;
import Dictionary;
import DictionaryGenerator;
import ProblemAnalyzer;
import IPlugin;
export import ITranslator;

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;
namespace fs = std::filesystem;

export {

	class NormalJsonTranslator : public ITranslator {
	protected:

        TransEngine m_transEngine;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;

        fs::path m_inputDir;
        fs::path m_inputCacheDir;
        fs::path m_outputDir;
        fs::path m_outputCacheDir;
        fs::path m_cacheDir;
        fs::path m_projectDir;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_targetLang;
        std::string m_dictDir;

        int m_totalSentences = 0;
        int m_completedSentences = 0;

        int m_threadsNum;
        int m_batchSize;
        int m_contextHistorySize;
        int m_maxRetries;
        int m_saveCacheInterval;
        int m_apiTimeOutMs;
        bool m_checkQuota;
        bool m_smartRetry;
        bool m_usePreDictInName;
        bool m_usePostDictInName;
        bool m_usePreDictInMsg;
        bool m_usePostDictInMsg;
        bool m_useGPTDictToReplaceName;
        bool m_outputWithSrc;

        std::string m_apiStrategy;
        std::string m_sortMethod;
        std::string m_splitFile;
        int m_splitFileNum;
        std::string m_linebreakSymbol;
        std::vector<std::string> m_retranslKeys;

        bool m_needsCombining = false;
        // 输入分割文件相对路径到原始json相对路径的映射
        std::map<fs::path, fs::path> m_splitFilePartsToJson;
        // 原始json相对路径到多个输入分割文件相对路径及其有没有完成的映射
        std::map<fs::path, std::map<fs::path, bool>> m_jsonToSplitFileParts;

        std::map<std::string, std::string> m_nameMap;
        std::mutex m_cacheMutex;
        std::mutex m_outputCacheFileMutex;
        toml::table m_problemOverview = toml::table{ {"problemOverview", toml::array{}} };
        std::function<void(fs::path)> m_onFileProcessed;

        APIPool m_apiPool;
        GptDictionary m_gptDictionary;
        NormalDictionary m_preDictionary;
        NormalDictionary m_postDictionary;
        ProblemAnalyzer m_problemAnalyzer;
        std::vector<std::shared_ptr<IPlugin>> m_prePlugins;
        std::vector<std::shared_ptr<IPlugin>> m_postPlugins;
        

    private:
        void preProcess(Sentence* se);

        void postProcess(Sentence* se);

        bool translateBatchWithRetry(const fs::path& relInputPath, std::vector<Sentence*>& batch, int threadId);

        void processFile(const fs::path& inputPath, int threadId);

	public:
        NormalJsonTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger,
            std::optional<fs::path> inputDir = std::nullopt, std::optional<fs::path> inputCacheDir = std::nullopt,
            std::optional<fs::path> outputDir = std::nullopt, std::optional<fs::path> outputCacheDir = std::nullopt);

        virtual ~NormalJsonTranslator()
        {
            m_logger->info("所有任务已完成！NormalJsonTranlator结束。");
        }

        virtual void run() override;
	};
}

module :private;

NormalJsonTranslator::NormalJsonTranslator(const fs::path& projectDir, std::shared_ptr<IController> controller, std::shared_ptr<spdlog::logger> logger,
    std::optional<fs::path> inputDir, std::optional<fs::path> inputCacheDir,
    std::optional<fs::path> outputDir, std::optional<fs::path> outputCacheDir) :
    m_projectDir(projectDir), m_controller(controller), m_logger(logger),
    m_apiPool(logger), m_gptDictionary(logger), m_preDictionary(logger), m_postDictionary(logger), m_problemAnalyzer(logger)
{
    m_inputDir = inputDir.value_or(m_projectDir / L"gt_input");
    m_inputCacheDir = inputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_input_cache");
    m_outputDir = outputDir.value_or(m_projectDir / L"gt_output");
    m_outputCacheDir = outputCacheDir.value_or(L"cache" / m_projectDir.filename() / L"gt_output_cache");
    m_cacheDir = m_projectDir / L"transl_cache";

    std::ifstream ifs;
    fs::path configPath = m_projectDir / L"config.toml";
    if (!fs::exists(configPath)) {
        throw std::runtime_error("找不到 config.toml 文件");
    }
    try {
        ifs.open(configPath);
        auto configData = toml::parse(ifs);
        ifs.close();

        std::string transEngineStr = configData["plugins"]["transEngine"].value_or("ForGalJson");
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
            throw std::runtime_error("Invalid trans engine");
        }


        m_apiStrategy = configData["backendSpecific"]["OpenAI-Compatible"]["apiStrategy"].value_or("random");
        if (m_apiStrategy != "random" && m_apiStrategy != "fallback") {
            throw std::invalid_argument("apiStrategy must be random or fallback in config.toml");
        }
        int apiTimeOutSecond = configData["backendSpecific"]["OpenAI-Compatible"]["apiTimeout"].value_or(60);
        m_apiTimeOutMs = apiTimeOutSecond * 1000;

        // 需要API
        if (m_transEngine != TransEngine::DumpName && m_transEngine != TransEngine::Rebuild && m_transEngine != TransEngine::ShowNormal) {
            auto translationAPIs = configData["backendSpecific"]["OpenAI-Compatible"]["apis"].as_array();
            if (!translationAPIs) {
                throw std::invalid_argument("OpenAI-Compatible apis not found in config.toml");
            }
            std::vector<TranslationAPI> m_translationAPIs;
            translationAPIs->for_each([&](auto&& el)
                {
                    TranslationAPI translationAPI;
                    if constexpr (toml::is_table<decltype(el)>) {
                        if (auto value = el["apikey"].value<std::string>(); !((*value).empty())) {
                            translationAPI.apikey = *value;
                        }
                        else if (m_transEngine == TransEngine::Sakura) {
                            translationAPI.apikey = "sk-sakura";
                        }
                        else{
                            return;
                        }
                        if (auto value = el["apiurl"].value<std::string>(); !((*value).empty())) {
                            translationAPI.apiurl = cvt2StdApiUrl(value.value());
                        }
                        else {
                            return;
                        }
                        if (auto value = el["modelName"].value<std::string>(); !((*value).empty())) {
                            translationAPI.modelName = *value;
                        }
                        else if (m_transEngine == TransEngine::Sakura) {
                            translationAPI.modelName = "sakura";
                        }
                        else{
                            return;
                        }
                        translationAPI.stream = el["stream"].value_or(false);
                        translationAPI.lastReportTime = std::chrono::steady_clock::now();
                        m_translationAPIs.push_back(translationAPI);
                    }
                });

            if (m_translationAPIs.empty()) {
                throw std::invalid_argument("config.toml 中找不到可用的 apikey ");
            }
            else {
                m_apiPool.loadAPIs(m_translationAPIs);
            }
        }

        auto textPrePlugins = configData["plugins"]["textPrePlugins"].as_array();
        if (textPrePlugins) {
            std::vector<std::string> pluginNames;
            textPrePlugins->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        pluginNames.push_back(*el);
                    }
                });
            m_prePlugins = registerPlugins(pluginNames, m_projectDir, m_logger);
        }

        // 集中的插件配置
        auto textPostPlugins = configData["plugins"]["textPostPlugins"].as_array();
        if (textPostPlugins) {
            std::vector<std::string> pluginNames;
            textPostPlugins->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        pluginNames.push_back(*el);
                    }
                });
            m_postPlugins = registerPlugins(pluginNames, m_projectDir, m_logger);
        }

        ifs.open(pluginConfigsPath / L"filePlugins/NormalJson.toml");
        auto pluginConfigData = toml::parse(ifs);
        ifs.close();

        m_outputWithSrc = parseToml<bool>(configData, pluginConfigData, "plugins.NormalJson.output_with_src");

        m_batchSize = configData["common"]["numPerRequestTranslate"].value_or(8);
        m_threadsNum = configData["common"]["threadsNum"].value_or(1);
        m_sortMethod = configData["common"]["sortMethod"].value_or("name");
        m_targetLang = configData["common"]["targetLang"].value_or("zh-cn");
        m_splitFile = configData["common"]["splitFile"].value_or("no");
        m_splitFileNum = configData["common"]["splitFileNum"].value_or(25);
        m_saveCacheInterval = configData["common"]["saveCacheInterval"].value_or(1);
        m_linebreakSymbol = configData["common"]["linebreakSymbol"].value_or("auto");
        m_maxRetries = configData["common"]["maxRetries"].value_or(5);
        m_contextHistorySize = configData["common"]["contextHistorySize"].value_or(8);
        m_smartRetry = configData["common"]["smartRetry"].value_or(true);
        m_checkQuota = configData["common"]["checkQuota"].value_or(true);
        m_dictDir = configData["common"]["dictDir"].value_or("DictGenerator/mecab-ipadic-utf8");

        auto retranslKeys = configData["common"]["retranslKeys"].as_array();
        if (retranslKeys) {
            retranslKeys->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        if ((*el).empty()) {
                            return;
                        }
                        m_retranslKeys.push_back(*el);
                    }
                });
        }

        auto problemList = configData["problemAnalyze"]["problemList"].as_array();
        if (problemList) {
            std::vector<std::string> problemsToCheck;
            problemList->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        problemsToCheck.push_back(*el);
                    }
                });
            std::string punctSet = configData["problemAnalyze"]["punctSet"].value_or("");
            double langProbability = configData["problemAnalyze"]["langProbability"].value_or(0.85);
            m_problemAnalyzer.loadProblems(problemsToCheck, punctSet, langProbability);
        }

        std::string defaultDictFolder = configData["dictionary"]["defaultDictFolder"].value_or("Dict");
        fs::path defaultDictFolderPath = ascii2Wide(defaultDictFolder);

        m_usePreDictInName = configData["dictionary"]["usePreDictInName"].value_or(false);
        m_usePostDictInName = configData["dictionary"]["usePostDictInName"].value_or(false);
        m_usePreDictInMsg = configData["dictionary"]["usePreDictInMsg"].value_or(true);
        m_usePostDictInMsg = configData["dictionary"]["usePostDictInMsg"].value_or(true);
        m_useGPTDictToReplaceName = configData["dictionary"]["useGPTDictInName"].value_or(false);

        m_gptDictionary.createTagger(ascii2Wide(m_dictDir));
        auto gptDicts = configData["dictionary"]["gptDict"].as_array();
        if (gptDicts) {
            gptDicts->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        fs::path dictPath = m_projectDir / ascii2Wide(*el);
                        if (!fs::exists(dictPath)) {
                            dictPath = defaultDictFolderPath / L"gpt" / ascii2Wide(*el);
                        }
                        if (fs::exists(dictPath)) {
                            m_gptDictionary.loadFromFile(dictPath);
                        }
                    }
                });
        }

        auto preDicts = configData["dictionary"]["preDict"].as_array();
        if (preDicts) {
            preDicts->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        fs::path dictPath = m_projectDir / ascii2Wide(*el);
                        if (!fs::exists(dictPath)) {
                            dictPath = defaultDictFolderPath / L"pre" / ascii2Wide(*el);
                        }
                        if (fs::exists(dictPath)) {
                            m_preDictionary.loadFromFile(dictPath);
                        }
                    }
                });
        }

        auto postDicts = configData["dictionary"]["postDict"].as_array();
        if (postDicts) {
            postDicts->for_each([&](auto&& el)
                {
                    if constexpr (toml::is_string<decltype(el)>) {
                        fs::path dictPath = m_projectDir / ascii2Wide(*el);
                        if (!fs::exists(dictPath)) {
                            dictPath = defaultDictFolderPath / L"post" / ascii2Wide(*el);
                        }
                        if (fs::exists(dictPath)) {
                            m_postDictionary.loadFromFile(dictPath);
                        }
                    }
                });
        }

        m_preDictionary.sort();
        m_gptDictionary.sort();
        m_postDictionary.sort();

        if (m_transEngine == TransEngine::DumpName || m_transEngine == TransEngine::Rebuild || m_transEngine == TransEngine::ShowNormal) {
            // 这几个不需要加载提示词
            return;
        }

        // 加载提示词
        fs::path promptPath = m_projectDir / L"Prompt.toml";
        if (!fs::exists(promptPath)) {
            promptPath = L"BaseConfig/Prompt.toml";
            if (!fs::exists(promptPath)) {
                throw std::runtime_error("找不到 Prompt.toml 文件");
            }
        }
        ifs.open(promptPath);
        auto promptData = toml::parse(ifs);
        ifs.close();

        std::string systemKey;
        std::string userKey;

        switch (m_transEngine) {
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
        default:
            throw std::invalid_argument("未知的 TransEngine");
        }

        if (auto value = promptData[systemKey].value<std::string>()) {
            m_systemPrompt = *value;
        }
        else {
            throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", systemKey));
        }
        if (auto value = promptData[userKey].value<std::string>()) {
            m_userPrompt = *value;
        }
        else {
            throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", userKey));
        }
    }
    catch (const toml::parse_error& e) {
        m_logger->critical("项目配置文件解析失败, 错误位置: {}, 错误信息: {}", stream2String(e.source().begin), e.description());
        throw std::runtime_error(e.what());
    }
}

void NormalJsonTranslator::preProcess(Sentence* se) {

    // se->name = se->name;
    // 相当于省略了 name_org 这一项，因为最原始人名并不在缓存里输出
    // se->name 实际上相当于 se->name_preproc
    se->pre_processed_text = se->original_text;

    if (se->hasName && m_usePreDictInName) {
        se->name = m_preDictionary.doReplace(se, CachePart::Name);
    }

    if (m_linebreakSymbol == "auto") {
        if (se->pre_processed_text.find("<br>") != std::string::npos) {
            se->originalLinebreak = "<br>";
        }
        else if (se->pre_processed_text.find("\\r\\n") != std::string::npos) {
            se->originalLinebreak = "\\r\\n";
        }
        else if (se->pre_processed_text.find("\\n") != std::string::npos) {
            se->originalLinebreak = "\\n";
        }
        else if (se->pre_processed_text.find("\\r") != std::string::npos) {
            se->originalLinebreak = "\\r";
        }
        else if (se->pre_processed_text.find("\r\n") != std::string::npos) {
            se->originalLinebreak = "\r\n";
        }
        else if (se->pre_processed_text.find("\n") != std::string::npos) {
            se->originalLinebreak = "\n";
        }
        else if (se->pre_processed_text.find("\r") != std::string::npos) {
            se->originalLinebreak = "\r";
        }
        else if (se->pre_processed_text.find("[r][n]") != std::string::npos) {
            se->originalLinebreak = "[r][n]";
        }
        else if (se->pre_processed_text.find("[n]") != std::string::npos) {
            se->originalLinebreak = "[n]";
        }
        else if (se->pre_processed_text.find("[r]") != std::string::npos) {
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
        se->pre_processed_text = m_preDictionary.doReplace(se, CachePart::PreprocText);
    }

    for (const auto& plugin : m_prePlugins) {
        plugin->run(se);
        if (se->complete) {
            break;
        }
    }

}

void NormalJsonTranslator::postProcess(Sentence* se) {

    se->name_preview = se->name;
    se->translated_preview = se->pre_translated_text;

    for (auto& plugin : m_postPlugins) {
        plugin->run(se);
    }

    if (m_usePostDictInMsg) {
        se->translated_preview = m_postDictionary.doReplace(se, CachePart::TransPreview);
    }
    replaceStrInplace(se->translated_preview, "<tab>", "\t");
    if (!se->originalLinebreak.empty()) {
        replaceStrInplace(se->translated_preview, "<br>", se->originalLinebreak);
    }

    if (se->hasName) {
        if (m_useGPTDictToReplaceName) {
            se->name_preview = m_gptDictionary.doReplace(se, CachePart::NamePreview);
        }
        if (!se->name_preview.empty()) {
            auto it = m_nameMap.find(se->name_preview);
            if (it != m_nameMap.end() && !it->second.empty()) {
                se->name_preview = it->second;
            }
        }
        if (m_usePostDictInName) {
            se->name_preview = m_postDictionary.doReplace(se, CachePart::NamePreview);
        }
    }

    m_problemAnalyzer.analyze(se, m_gptDictionary, m_targetLang);
}


bool NormalJsonTranslator::translateBatchWithRetry(const fs::path& relInputPath, std::vector<Sentence*>& batch, int threadId) {

    if (batch.empty()) {
        return true;
    }
    for (auto& pSentence : batch) {
        if (pSentence->pre_processed_text.empty()) {
            pSentence->complete = true;
            m_completedSentences++;
            m_controller->updateBar(); // 为空不翻译
        }
    }

    int retryCount = 0;
    std::string contextHistory = buildContextHistory(batch, m_transEngine, m_contextHistorySize);
    std::string glossary = m_gptDictionary.generatePrompt(batch, m_transEngine);

    while (retryCount < m_maxRetries) {

        if (m_controller->shouldStop()) {
            return false;
        }

        std::vector<Sentence*> batchToTransThisRound;
        for (auto pSentence : batch) {
            if (pSentence->complete) {
                continue;
            }
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

            bool firstOk = translateBatchWithRetry(relInputPath, firstHalf, threadId);
            bool secondOk = translateBatchWithRetry(relInputPath, secondHalf, threadId);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] [文件 {}] 清空上下文后再次尝试...", threadId, wide2Ascii(relInputPath));
            contextHistory.clear();
        }


        std::string inputProblems;
        for (auto pSentence : batchToTransThisRound) {
            if (!pSentence->problem.empty() && inputProblems.find(pSentence->problem) == std::string::npos) {
                inputProblems += pSentence->problem + "\n";
            }
        }

        std::string inputBlock;
        std::map<int, Sentence*> id2SentenceMap; // 用于 TSV/JSON 
        fillBlockAndMap(batchToTransThisRound, id2SentenceMap, inputBlock, m_transEngine);

        m_logger->info("[线程 {}] [文件 {}] 开始翻译\nProblems:\n{}\nDict:\n{}\ninputBlock:\n{}", threadId, wide2Ascii(relInputPath), inputProblems, glossary, inputBlock);
        std::string promptReq = m_userPrompt;
        promptReq = boost::regex_replace(promptReq, boost::regex(R"(\[Problem Description\])"), inputProblems);
        promptReq = boost::regex_replace(promptReq, boost::regex(R"(\[Input\])"), inputBlock);
        promptReq = boost::regex_replace(promptReq, boost::regex(R"(\[TargetLang\])"), m_targetLang);
        promptReq = boost::regex_replace(promptReq, boost::regex(R"(\[Glossary\])"), glossary);

        json messages = json::array({ {{"role", "system"}, {"content", m_systemPrompt}} });
        if (!contextHistory.empty()) {
            messages.push_back({ {"role", "user"}, {"content", "<input>(...truncated history source texts...)</input><output>\n"} });
            messages.push_back({ {"role", "assistant"}, {"content", contextHistory} });
        }
        messages.push_back({ {"role", "user"}, {"content", promptReq} });

        auto optAPI = m_apiStrategy == "random" ? m_apiPool.getAPI() : m_apiPool.getFirstAPI();
        if (!optAPI.has_value()) {
            throw std::runtime_error("没有可用的API Key了");
        }
        TranslationAPI currentAPI = optAPI.value();

        json payload = { {"model", currentAPI.modelName}, {"messages", messages} };

        ApiResponse response = performApiRequest(payload, currentAPI, threadId, m_controller, m_logger, m_apiTimeOutMs);
        if (!response.success) {

            std::string lowerErrorMsg = response.content;
            std::transform(lowerErrorMsg.begin(), lowerErrorMsg.end(), lowerErrorMsg.begin(), ::tolower);

            // 情况一：额度用尽 (Quota)
            if (
                m_checkQuota &&
                (lowerErrorMsg.find("quota") != std::string::npos ||
                    lowerErrorMsg.find("invalid tokens") != std::string::npos)
                )
            {
                m_logger->error("[线程 {}] API Key [{}] 疑似额度用尽，短期内多次报告将从池中移除。", threadId, currentAPI.apikey);
                m_apiPool.reportProblem(currentAPI);
                // 不需要增加 retryCount
                continue;
            }
            // key 没有这个模型
            else if (lowerErrorMsg.find("no available") != std::string::npos) {
                m_logger->error("[线程 {}] API Key [{}] 没有 [{}] 模型，短期内多次报告将从池中移除。", threadId, currentAPI.apikey, currentAPI.modelName);
                m_apiPool.reportProblem(currentAPI);
                continue;
            }

            // 情况二：频率限制 (429) 或其他可重试错误
            // 状态码 429 是最明确的信号
            if (response.statusCode == 429 || lowerErrorMsg.find("rate limit") != std::string::npos || lowerErrorMsg.find("try again") != std::string::npos) {
                retryCount++;
                m_logger->warn("[线程 {}] [文件 {}] 遇到频率限制或可重试错误，进行第 {} 次退避等待...", threadId, wide2Ascii(relInputPath.filename()), retryCount);

                // 实现指数退避与抖动
                int sleepSeconds;
                int maxSleepSeconds = static_cast<int>(std::pow(2, std::min(retryCount, 6)));
                if (maxSleepSeconds > 0) {
                    sleepSeconds = std::rand() % maxSleepSeconds;
                }
                else {
                    sleepSeconds = 0;
                }
                m_logger->debug("[线程 {}] 将等待 {} 秒后重试...", threadId, sleepSeconds);
                if (sleepSeconds > 0) {
                    std::this_thread::sleep_for(std::chrono::seconds(sleepSeconds));
                }
                continue;
            }

            // 其他无法识别的硬性错误
            retryCount++;
            m_logger->warn("[线程 {}] [文件 {}] 遇到未知API错误，进行第 {} 次重试...", threadId, wide2Ascii(relInputPath.filename()), retryCount);
            if (m_apiStrategy == "fallback") {
                m_logger->warn("[线程 {}] 将切换到下一个 API Key(如果有多个API Key的话)", threadId);
                m_apiPool.resortTokens();
            }
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
            continue;
        }

        // --- 如果请求成功，则继续解析 ---
        std::string content = response.content;
        m_logger->info("[线程 {}] [文件 {}] 成功响应，解析结果: \n{}", threadId, wide2Ascii(relInputPath), content);
        int parsedCount = 0;
        bool parseError = false;

        parseContent(content, batchToTransThisRound, id2SentenceMap, currentAPI.modelName, m_transEngine, parseError, parsedCount,
            m_controller, m_completedSentences);

        if (parseError || parsedCount != batchToTransThisRound.size()) {
            retryCount++;
            m_logger->warn("[线程 {}] [文件 {}] 解析失败或不完整 ({} / {}), 进行第 {} 次重试...", threadId, wide2Ascii(relInputPath.filename()), parsedCount, batchToTransThisRound.size(), retryCount);
            continue;
        }

        m_logger->debug("[线程 {}] 批次翻译成功，解析了 {} 句话。", threadId, parsedCount);
        return true;
    }

    m_logger->error("[线程 {}] [文件 {}] 批次翻译在 {} 次重试后彻底失败。", threadId, wide2Ascii(relInputPath.filename()), m_maxRetries);
    for (auto& pSentence : batch) {
        if (pSentence->complete) {
            continue;
        }
        pSentence->pre_translated_text = "(Failed to translate)" + pSentence->original_text;
        pSentence->complete = true;
        m_completedSentences++;
        m_controller->updateBar(); // 失败
    }
    return false;
}


// ============================================        processFile        ========================================
void NormalJsonTranslator::processFile(const fs::path& inputPath, int threadId) {
    if (m_controller->shouldStop()) {
        return;
    }
    m_logger->debug("[线程 {}] 开始处理文件: {}", threadId, wide2Ascii(inputPath));
    m_controller->addThreadNum();

    std::ifstream ifs;
    fs::path relInputPath = fs::relative(inputPath, m_needsCombining ? m_inputCacheDir : m_inputDir);
    fs::path outputPath = m_needsCombining ? (m_outputCacheDir / relInputPath) : (m_outputDir / relInputPath);
    fs::path cachePath = m_cacheDir / relInputPath;
    fs::path showNormalPath = m_projectDir / L"gt_show_normal" / relInputPath;
    createParent(outputPath);
    createParent(cachePath);

    std::vector<Sentence> sentences;
    try {
        ifs.open(inputPath);
        json data = json::parse(ifs);
        ifs.close();
        for (size_t i = 0; i < data.size(); ++i) {
            const auto& item = data[i];
            Sentence se;
            se.index = (int)i;
            if (item.contains("name")) {
                se.hasName = true;
                se.name = item.value("name", "");
            }
            if (!item.contains("message")) {
                throw std::runtime_error(std::format("[线程 {}] [文件 {}] 第 {} 个对象缺少 message 字段。", threadId, wide2Ascii(inputPath), i));
            }
            se.original_text = item.value("message", "");
            sentences.push_back(se);
        }
        for (size_t i = 0; i < sentences.size(); ++i) {
            if (i > 0) sentences[i].prev = &sentences[i - 1];
            if (i + 1 < sentences.size()) sentences[i].next = &sentences[i + 1];
        }
        for (auto& se : sentences) {
            preProcess(&se);
        }
    }
    catch (const json::exception& e) {
        throw std::runtime_error(std::format("[线程 {}] [文件 {}] 解析失败: {}", threadId, wide2Ascii(inputPath), e.what()));
    }

    if (m_transEngine == TransEngine::ShowNormal) {
        json showNormalJson = json::array();
        for (const auto& se : sentences) {
            json showNormalObj;
            showNormalObj["name"] = se.name;
            showNormalObj["original_text"] = se.original_text;
            if (!se.other_info.empty()) {
                showNormalObj["other_info"] = se.other_info;
            }
            showNormalObj["pre_processed_text"] = se.pre_processed_text;
            showNormalJson.push_back(showNormalObj);
            m_completedSentences++;
            m_controller->updateBar(); // ShowNormal
        }
        createParent(showNormalPath);
        std::ofstream ofs(showNormalPath);
        ofs << showNormalJson.dump(2);
        return;
    }

    std::vector<Sentence*> toTranslate;

    {
        std::map<std::string, json> cacheMap;

        auto insertCacheMap = [&cacheMap](const json& jsonArr)
            {
                for (size_t i = 0; i < jsonArr.size(); ++i) {
                    const auto& item = jsonArr[i];
                    std::string prevText = "None", currentText, nextText = "None";
                    currentText = item.value("name", "") + item.value("original_text", "") + item.value("pre_processed_text", "");
                    if (i > 0) prevText = jsonArr[i - 1].value("name", "") + jsonArr[i - 1].value("original_text", "") + jsonArr[i - 1].value("pre_processed_text", "");
                    if (i + 1 < jsonArr.size()) nextText = jsonArr[i + 1].value("name", "") + jsonArr[i + 1].value("original_text", "") + jsonArr[i + 1].value("pre_processed_text", "");
                    cacheMap.insert(std::make_pair(prevText + currentText + nextText, item));
                }
            };

        std::vector<fs::path> cachePaths;
        if (m_needsCombining && m_transEngine != TransEngine::Rebuild) {
            // 这个逻辑还挺耗时的
            size_t pos = relInputPath.filename().wstring().rfind(L"_part_");
            std::wstring orgStem = relInputPath.filename().wstring().substr(0, pos);
            std::wstring cacheSpec = orgStem + L"_part_*.json";
            for (const auto& entry : fs::directory_iterator(m_cacheDir / relInputPath.parent_path())) {
                if (!entry.is_regular_file()) {
                    continue;
                }
                if (PathMatchSpecW(entry.path().filename().wstring().c_str(), cacheSpec.c_str())) {
                    cachePaths.push_back(entry.path());
                }
            }

            for (const auto& cp : cachePaths) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                try {
                    ifs.open(cp);
                    json cacheJsonList = json::parse(ifs);
                    ifs.close();
                    m_logger->debug("[线程 {}] 从 {} 加载了 {} 条缓存记录。", threadId, wide2Ascii(cp), cacheMap.size());
                    insertCacheMap(cacheJsonList);
                }
                catch (const json::exception& e) {
                    throw std::runtime_error(std::format("[线程 {}] 缓存文件 {} 解析失败: {}", threadId, wide2Ascii(cp), e.what()));
                }
            }
        }
        else if (fs::exists(cachePath)) {
            cachePaths.push_back(cachePath);
        }

        json totalCacheJsonList = json::array();
        for (const auto& cp : cachePaths) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            try {
                ifs.open(cp);
                json cacheJsonList = json::parse(ifs);
                ifs.close();
                totalCacheJsonList.insert(totalCacheJsonList.end(), cacheJsonList.begin(), cacheJsonList.end());
                m_logger->debug("[线程 {}] 从 {} 加载了 {} 条缓存记录。", threadId, wide2Ascii(cp), cacheMap.size());
            }
            catch (const json::exception& e) {
                throw std::runtime_error(std::format("[线程 {}] 缓存文件 {} 解析失败: {}", threadId, wide2Ascii(cp), e.what()));
            }
        }
        insertCacheMap(totalCacheJsonList);


        for (auto& se : sentences) {
            if (se.complete) {
                m_completedSentences++;
                m_controller->updateBar(); // 跳过已完成的句子
                postProcess(&se);
                continue;
            }
            std::string key = generateCacheKey(&se);
            auto it = cacheMap.find(key);
            if (it == cacheMap.end()) {
                toTranslate.push_back(&se);
                continue;
            }
            const auto& item = it->second;
            se.problem = item.value("problem", "");
            if (m_transEngine != TransEngine::Rebuild && hasRetranslKey(m_retranslKeys, &se)) {
                toTranslate.push_back(&se);
                continue;
            }
            se.pre_translated_text = item.value("pre_translated_text", "");
            se.translated_by = item.value("translated_by", "");
            se.complete = true;
            m_completedSentences++;
            m_controller->updateBar(); // 命中缓存
            postProcess(&se);
        }

        m_logger->info("[线程 {}] [文件 {}] 共 {} 句，命中缓存 {} 句，需翻译 {} 句。", threadId, wide2Ascii(relInputPath),
            sentences.size(), sentences.size() - toTranslate.size(), toTranslate.size());

        if (m_transEngine == TransEngine::Rebuild && !toTranslate.empty()) {
            m_logger->critical("[线程 {}] [文件 {}] 有 {} 句未命中缓存，这些句子是: ", threadId, wide2Ascii(relInputPath), toTranslate.size());
            for (auto& se : toTranslate) {
                m_logger->error("[{}]", se->original_text);
            }
            saveCache(sentences, cachePath);
            m_controller->reduceThreadNum();
            return;
        }
    }

    int batchCount = 0;
    for (size_t i = 0; i < toTranslate.size(); i += m_batchSize) {
        if (m_controller->shouldStop()) {
            m_controller->reduceThreadNum();
            return;
        }
        std::vector<Sentence*> batch(toTranslate.begin() + i, toTranslate.begin() + std::min(i + m_batchSize, toTranslate.size()));
        translateBatchWithRetry(relInputPath, batch, threadId);
        for (auto& se : batch) {
            postProcess(se);
        }
        batchCount++;
        if (batchCount % m_saveCacheInterval == 0) {
            m_logger->debug("[线程 {}] [文件 {}] 达到保存间隔，正在更新缓存文件...", threadId, wide2Ascii(inputPath));
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            saveCache(sentences, cachePath);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_logger->debug("[线程 {}] [文件 {}] 翻译完成，正在进行最终保存...", threadId, wide2Ascii(inputPath));
        saveCache(sentences, cachePath);
        auto overviewArr = m_problemOverview["problemOverview"].as_array();
        if (!overviewArr) {
            throw std::runtime_error("problemOverview 字段不是数组");
        }
        std::string relInputPathStr = wide2Ascii(relInputPath);
        for (const auto& se : sentences) {
            if (se.problem.empty()) {
                continue;
            }
            toml::table tbl;
            tbl.insert("filename", relInputPathStr);
            tbl.insert("index", se.index);
            tbl.insert("name", se.name);
            tbl.insert("name_preview", se.name_preview);
            tbl.insert("original_text", se.original_text);
            if (!se.other_info.empty()) {
                toml::table otherInfoArr;
                for (const auto& [k, v] : se.other_info) {
                    otherInfoArr.insert(k, v);
                }
                tbl.insert("other_info", otherInfoArr);
            }
            tbl.insert("pre_processed_text", se.pre_processed_text);
            tbl.insert("pre_translated_text", se.pre_translated_text);
            tbl.insert("problem", se.problem);
            tbl.insert("translated_by", se.translated_by);
            tbl.insert("translated_preview", se.translated_preview);
            overviewArr->push_back(tbl);
        }
    }

    ordered_json outputJson = ordered_json::array();
    for (const auto& se : sentences) {
        ordered_json obj;
        if (se.hasName) {
            obj["name"] = se.name_preview;
        }
        obj["message"] = se.translated_preview;
        if (m_outputWithSrc) {
            obj["src_msg"] = se.original_text;
        }
        outputJson.push_back(obj);
    }

    std::lock_guard<std::mutex> lock(m_outputCacheFileMutex);
    std::ofstream ofs(outputPath);
    ofs << outputJson.dump(2);
    ofs.close();

    m_logger->info("[线程 {}] [文件 {}] 处理完成。", threadId, wide2Ascii(relInputPath));
    m_controller->reduceThreadNum();

    if (m_needsCombining) {
        fs::path originalRelFilePath = m_splitFilePartsToJson[relInputPath];
        auto& splitFileParts = m_jsonToSplitFileParts[originalRelFilePath];
        splitFileParts[relInputPath] = true;
        if (
            std::ranges::any_of(splitFileParts, [](const auto& p) { return !p.second; })
            )
        {
            m_logger->debug("文件 {} 尚未全部处理完成，跳过合并。", wide2Ascii(relInputPath));
            return;
        }
        m_logger->debug("开始合并 {} 的缓存文件...", wide2Ascii(relInputPath));
        combineOutputFiles(originalRelFilePath, splitFileParts, m_logger, m_outputCacheDir, m_outputDir);

        if (m_onFileProcessed) {
            m_onFileProcessed(originalRelFilePath);
        }
    }
    else {
        if (m_onFileProcessed) {
            m_onFileProcessed(relInputPath);
        }
    }
}

// ================================================         run           ========================================
void NormalJsonTranslator::run() {
    m_logger->info("GalTransl++ NormalJsonTranlator 启动...");

    for (const auto& dir : { m_inputDir, m_outputDir, m_cacheDir }) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            m_logger->debug("已创建目录: {}", wide2Ascii(dir));
        }
    }

    fs::remove_all(m_inputCacheDir);
    fs::remove_all(m_outputCacheDir);

    std::ifstream ifs;
    std::ofstream ofs;

    std::map<std::string, int> nameTableMap;
    bool needGenerateNameTable = m_transEngine == TransEngine::DumpName || !fs::exists(m_projectDir / L"人名替换表.toml");

    Sentence se;
    for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
        if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
            continue;
        }
        try {
            ifs.open(entry.path());
            json data = json::parse(ifs);
            ifs.close();
            for (const auto& item : data) {
                m_totalSentences++;
                if (!needGenerateNameTable || !item.contains("name")) {
                    continue;
                }
                se.name = item["name"].get<std::string>();
                if (m_usePreDictInName) {
                    se.name = m_preDictionary.doReplace(&se, CachePart::Name);
                }
                if (se.name.empty()) {
                    continue;
                }
                auto it = nameTableMap.find(se.name);
                if (it == nameTableMap.end()) {
                    nameTableMap.insert(std::make_pair(se.name, 1));
                }
                else {
                    it->second++;
                }
            }
        }
        catch (const json::exception& e) {
            m_logger->critical("读取文件 {} 时出错", wide2Ascii(entry.path()));
            throw std::runtime_error(e.what());
        }
    }

    m_controller->makeBar(m_totalSentences, m_threadsNum);

    if (needGenerateNameTable) {
        std::vector<std::string> nameTableKeys;
        for (const auto& [name, count] : nameTableMap) {
            nameTableKeys.push_back(name);
        }
        std::ranges::sort(nameTableKeys, [&](const std::string& a, const std::string& b)
            {
                return nameTableMap[a] > nameTableMap[b];
            });

        ofs.open(m_projectDir / L"人名替换表.toml");
        ofs << "# '原名' = [ '译名', 出现次数 ]" << std::endl;
        for (const auto& key : nameTableKeys) {
            auto nameTable = toml::table{ {key, toml::array{ "", nameTableMap[key] }} };
            ofs << nameTable << std::endl;
        }
        ofs.close();
        m_logger->info("已生成 人名替换表.toml 文件");
        if (m_transEngine == TransEngine::DumpName) {
            m_completedSentences += m_totalSentences;
            m_controller->updateBar(m_totalSentences);
            return;
        }
    }

    if (m_transEngine == TransEngine::GenDict) {
        DictionaryGenerator generator(m_controller, m_logger, m_apiPool, ascii2Wide(m_dictDir), m_systemPrompt, m_userPrompt, m_apiStrategy,
            m_maxRetries, m_threadsNum, m_apiTimeOutMs, m_checkQuota);
        fs::path outputFilePath = m_projectDir / L"项目GPT字典-生成.toml";
        generator.generate(m_inputDir, outputFilePath, m_preDictionary, m_usePreDictInName);
        m_logger->info("已生成 项目GPT字典-生成.toml 文件");
        return;
    }

    try {
        ifs.open(m_projectDir / L"人名替换表.toml");
        auto nameTable = toml::parse(ifs);
        ifs.close();

        for (const auto& [key, value] : nameTable) {
            auto arr = value.as_array();
            if (!arr || arr->size() < 1) {
                continue;
            }
            std::string transName = arr->get(0)->value_or("");
            if (!transName.empty()) {
                m_logger->trace("发现原名 '{}' 的译名 '{}'", key.str(), transName);
                m_nameMap.insert(std::make_pair(key.str(), transName));
            }
        }
    }
    catch (const toml::parse_error& e) {
        m_logger->critical("解析 人名替换表.toml 时出错");
        throw std::runtime_error(e.what());
    }

    fs::path sourceDir = m_inputDir; // 默认从输入目录读取

    if (m_splitFile == "Equal" && m_splitFileNum > 1) {
        m_needsCombining = true;
        sourceDir = m_inputCacheDir;
        for (const auto& dir : { m_inputCacheDir, m_outputCacheDir }) {
            if (!fs::exists(dir)) {
                fs::create_directories(dir);
                m_logger->info("已创建目录: {}", wide2Ascii(dir));
            }
        }

        m_logger->info("检测到文件分割模式 (Equal)，开始预处理输入文件...");
        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }

            try {
                ifs.open(entry.path());
                json data = json::parse(ifs);
                ifs.close();

                std::vector<json> parts = splitJsonArrayEqual(data, m_splitFileNum);
                fs::path relWholePath = fs::relative(entry.path(), m_inputDir); // 原始json相对路径

                std::wstring stem = fs::relative(entry.path(), m_inputDir).parent_path() / entry.path().stem();
                for (size_t i = 0; i < parts.size(); ++i) {
                    fs::path relPartPath = stem + L"_part_" + std::to_wstring(i) + entry.path().extension().wstring();
                    m_splitFilePartsToJson[relPartPath] = relWholePath;
                    m_jsonToSplitFileParts[relWholePath].insert(std::make_pair(relPartPath, false));
                    fs::path partPath = m_inputCacheDir / relPartPath;
                    createParent(partPath);
                    ofs.open(partPath);
                    ofs << parts[i].dump(2);
                    ofs.close();
                }
                m_logger->debug("文件 {} 已被分割成 {} 份，存入输入缓存。", wide2Ascii(relWholePath), parts.size());

            }
            catch (const json::exception& e) {
                m_logger->critical("分割文件 {} 时出错", wide2Ascii(entry.path()));
                throw std::runtime_error(e.what());
            }
        }
    }
    else if (m_splitFile == "Num" && m_splitFileNum > 0) {
        m_needsCombining = true;
        sourceDir = m_inputCacheDir;
        for (const auto& dir : { m_inputCacheDir, m_outputCacheDir }) {
            if (!fs::exists(dir)) {
                fs::create_directories(dir);
                m_logger->info("已创建目录: {}", wide2Ascii(dir));
            }
        }

        m_logger->info("检测到文件分割模式 (Num)，开始预处理输入文件...");
        for (const auto& entry : fs::recursive_directory_iterator(m_inputDir)) {
            if (!entry.is_regular_file() || !isSameExtension(entry.path(), L".json")) {
                continue;
            }

            try {
                ifs.open(entry.path());
                json data = json::parse(ifs);
                ifs.close();

                fs::path relWholePath = fs::relative(entry.path(), m_inputDir); // 原始json相对路径
                std::vector<json> parts = splitJsonArrayNum(data, m_splitFileNum);

                std::wstring stem = fs::relative(entry.path(), m_inputDir).parent_path() / entry.path().stem();
                for (size_t i = 0; i < parts.size(); ++i) {
                    fs::path relPartPath = stem + L"_part_" + std::to_wstring(i) + entry.path().extension().wstring();
                    m_splitFilePartsToJson[relPartPath] = relWholePath;
                    m_jsonToSplitFileParts[relWholePath].insert(std::make_pair(relPartPath, false));
                    fs::path partPath = m_inputCacheDir / relPartPath;
                    createParent(partPath);
                    ofs.open(partPath);
                    ofs << parts[i].dump(2);
                    ofs.close();
                }
                m_logger->debug("文件 {} 已被按每 {} 句分割成 {} 份，存入输入缓存。",
                    wide2Ascii(relWholePath), m_splitFileNum, parts.size());

            }
            catch (const std::exception& e) {
                m_logger->critical("分割文件 {} 时出错", wide2Ascii(entry.path()));
                throw std::runtime_error(e.what());
            }
        }
    }
    else if (m_splitFile != "No") {
        throw std::invalid_argument(std::format("未知的文件分割模式: {}, 请使用 'No', 'Equal', 'Num'", m_splitFile));
    }


    m_logger->debug("开始从目录 {} 分发翻译任务...", wide2Ascii(sourceDir));
    std::vector<fs::path> filePaths;
    for (const auto& entry : fs::recursive_directory_iterator(sourceDir)) {
        if (entry.is_regular_file() && isSameExtension(entry.path(), L".json")) {
            filePaths.push_back(entry.path());
        }
    }
    if (filePaths.empty()) {
        throw std::runtime_error("未找到任何待翻译文件。");
    }

    if (m_sortMethod == "size") {
        std::ranges::sort(filePaths, [](const fs::path& a, const fs::path& b)
            {
                return fs::file_size(a) > fs::file_size(b);
            });
    }
    else if (m_sortMethod == "name") {
        std::ranges::sort(filePaths);
    }
    else {
        throw std::invalid_argument(std::format("未知的排序模式: {}", m_sortMethod));
    }


    ctpl::thread_pool pool(std::min(m_threadsNum, (int)filePaths.size()));
    std::vector<std::future<void>> results;

    for (const auto& filePath : filePaths) {
        results.emplace_back(pool.push([=](int id)
            {
                this->processFile(filePath, id);
            }));
    }

    m_logger->info("已将 {} 个文件任务分配到线程池，等待处理完成...", results.size());

    for (auto& result : results) {
        result.get();
    }

    auto overviewArr = m_problemOverview["problemOverview"].as_array();
    if (!overviewArr) {
        throw std::runtime_error("problemOverview 字段不是数组。");
    }
    if (overviewArr->empty()) {
        m_logger->info("\n\n```\n无问题概览\n```\n");
    }
    else {
        ofs.open(m_projectDir / L"翻译问题概览.toml");
        ofs << m_problemOverview;
        ofs.close();
        std::map<std::string, std::set<std::string>> problemMap;
        json problemOverviewJson = json::array();
        for (const auto& elem : *overviewArr) {
            auto problemSe = elem.as_table();
            if (!problemSe) {
                throw std::runtime_error("problemOverview 数组元素不是表。");
            }
            problemMap[problemSe->get("problem")->value_or(std::string{})]
                .insert(problemSe->get("filename")->value_or(std::string{}));
            std::stringstream ss;
            ss << toml::json_formatter(*problemSe);
            problemOverviewJson.push_back(
                json::parse(ss.str())
            );
        }
        ofs.open(m_projectDir / L"翻译问题概览.json");
        ofs << problemOverviewJson.dump(2);
        ofs.close();
        m_logger->debug("已生成 翻译问题概览.json 和 翻译问题概览.toml 文件");

        std::string problemOverviewStr = "\n\n```\n问题概览:\n";
        size_t problemCount = 0;
        for (const auto& [problem, files] : problemMap) {
            problemCount++;
            std::string fileStr = "(";
            size_t count = 0;
            for (const auto& file : files) {
                if (count == 3) {
                    break;
                }
                fileStr += file + ", ";
                count++;
            }
            if (count == files.size()) {
                fileStr.pop_back();
                fileStr.pop_back();
                fileStr += ")";
            }
            else {
                fileStr += "...)";
            }
            problemOverviewStr += std::format("{}. {}  |  {}\n", problemCount++, problem, fileStr);
        }
        m_logger->error("{}问题概览结束\n```\n", problemOverviewStr);
    }

    fs::remove_all(m_inputCacheDir);
    fs::remove_all(m_outputCacheDir);
    if (m_transEngine == TransEngine::Rebuild && m_completedSentences != m_totalSentences) {
        m_logger->critical("重建过程中有句子未命中缓存，请检查日志以定位问题。");
    }
}
