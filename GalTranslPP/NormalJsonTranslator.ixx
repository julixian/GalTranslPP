module;

#include <Windows.h>
#include <boost/regex.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <toml++/toml.hpp>
#include <Shlwapi.h>
#include <ctpl_stl.h>

import std;
import Tool;
import APIPool;
import Dictionary;
import DictionaryGenerator;
import ProblemAnalyzer;
import IPlugin;
export import ITranslator;
export module NormalJsonTranslator;

using json = nlohmann::json;
namespace fs = std::filesystem;

export {

    template<typename Mutex>
    class ControllerSink : public spdlog::sinks::base_sink<Mutex> {
    public:
        explicit ControllerSink(std::shared_ptr<IController> controller)
            : m_controller(controller) {
        }

    protected:

        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            this->formatter_->format(msg, formatted);
            m_controller->writeLog(fmt::to_string(formatted));
        }

        void flush_() override {
            // 也可以在IController里再写一个虚flush，不过感觉没什么必要了
        }

    private:
        std::shared_ptr<IController> m_controller;
    };


	class NormalJsonTranslator : public ITranslator {
	protected:

        TransEngine m_transEngine;
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<ControllerSink<std::mutex>> m_controllerSink;
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

        bool m_checkQuota = true;
        bool m_smartRetry = true;
        bool m_rebuildSuccess = true;
        bool m_usePreDictInName = false;
        bool m_usePostDictInName = false;
        bool m_usePreDictInMsg = true;
        bool m_usePostDictInMsg = true;
        bool m_useGPTDictToReplaceName = false;
        bool m_outputWithSrc = true;

        std::string m_apiStrategy;
        std::string m_sortMethod;
        std::string m_splitFile;
        int m_splitFileNum = 25;
        std::string m_linebreakSymbol = "auto";
        std::vector<std::string> m_retranslKeys;
        bool m_needsCombining = false;
        std::map<std::wstring, int> m_splitFilePartsCount;
        std::map<std::string, std::string> m_nameMap;
        std::mutex m_cacheMutex;
        std::mutex m_outputCacheFileMutex;

        APIPool m_apiPool;
        GptDictionary m_gptDictionary;
        NormalDictionary m_preDictionary;
        NormalDictionary m_postDictionary;
        ProblemAnalyzer m_problemAnalyzer;
        std::vector<std::shared_ptr<IPlugin>> m_postPlugins;
        

        void preProcess(Sentence* se);

        void postProcess(Sentence* se);

        std::string generateCacheKey(const Sentence* s);
        
        std::string buildContextHistory(const std::vector<Sentence*>& batch);

        ApiResponse performApiRequest(const json& payload, const TranslationAPI& api, int threadId);

        bool translateBatchWithRetry(std::vector<Sentence*>& batch, int threadId);

        void combineOutputFiles(const fs::path& originalFilename, int numParts, const fs::path& outputCacheDir, const fs::path& outputDir);

        bool isAllPartFilesReady(const fs::path& originalFilename, int numParts, const fs::path& outputCacheDir);

        void processFile(const fs::path& inputPath, int threadId);

	public:
        NormalJsonTranslator(const fs::path& projectDir, TransEngine transEngine, std::shared_ptr<IController> controller,
            std::optional<fs::path> inputDir = std::nullopt, std::optional<fs::path> inputCacheDir = std::nullopt,
            std::optional<fs::path> outputDir = std::nullopt, std::optional<fs::path> outputCacheDir = std::nullopt);

        virtual ~NormalJsonTranslator()
        {
            std::cout << std::endl;
        }

        virtual void run() override;
	};
}




module :private;

NormalJsonTranslator::NormalJsonTranslator(const fs::path& projectDir, TransEngine transEngine, std::shared_ptr<IController> controller,
    std::optional<fs::path> inputDir, std::optional<fs::path> inputCacheDir,
    std::optional<fs::path> outputDir, std::optional<fs::path> outputCacheDir) :
    m_projectDir(projectDir), m_transEngine(transEngine), m_controller(controller)
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


        // 日志配置
        spdlog::level::level_enum logLevel;
        bool saveLog = configData["common"]["saveLog"].value_or(true);
        std::string logLevelStr = configData["common"]["logLevel"].value_or("info");
        if (logLevelStr == "trace") {
            logLevel = spdlog::level::trace;
        }
        else if (logLevelStr == "debug") {
            logLevel = spdlog::level::debug;
        }
        else if (logLevelStr == "info") {
            logLevel = spdlog::level::info;
        }
        else if (logLevelStr == "warn") {
            logLevel = spdlog::level::warn;
        }
        else if (logLevelStr == "err") {
            logLevel = spdlog::level::err;
        }
        else if (logLevelStr == "critical") {
            logLevel = spdlog::level::critical;
        }
        else {
            throw std::runtime_error("Invalid log level");
        }

        m_controllerSink = std::make_shared<ControllerSink<std::mutex>>(m_controller);
        std::vector<spdlog::sink_ptr> sinks = { m_controllerSink };
        if (saveLog) {
            sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(wide2Ascii(m_projectDir, 0) + "/log.txt", true));
        }
        m_logger = std::make_shared<spdlog::logger>(wide2Ascii(m_projectDir) + "-Logger", sinks.begin(), sinks.end());
        m_logger->set_level(logLevel);
        m_logger->set_pattern("[%H:%M:%S.%e %^%l%$] %v");
        m_logger->info("Logger initialized.");
        m_apiPool.setLogger(m_logger);
        m_gptDictionary.setLogger(m_logger);
        m_preDictionary.setLogger(m_logger);
        m_postDictionary.setLogger(m_logger);
        m_problemAnalyzer.setLogger(m_logger);
        // 日志配置结束


        m_apiStrategy = configData["backendSpecific"]["OpenAI-Compatible"]["apiStrategy"].value_or("random");
        if (m_apiStrategy != "random" && m_apiStrategy != "fallback") {
            throw std::invalid_argument("apiStrategy must be random or fallback in config.toml");
        }
        int apiTimeOutSecond = configData["backendSpecific"]["OpenAI-Compatible"]["apiTimeout"].value_or(60);
        m_apiTimeOutMs = apiTimeOutSecond * 1000;

        std::vector<TranslationAPI> m_translationAPIs;
        auto translationAPIs = configData["backendSpecific"]["OpenAI-Compatible"]["apis"].as_array();
        if (!translationAPIs && transEngine != TransEngine::DumpName && transEngine != TransEngine::Rebuild) {
            throw std::invalid_argument("OpenAI-Compatible apis not found in config.toml");
        }
        translationAPIs->for_each([&](auto&& el)
            {
                TranslationAPI translationAPI;
                if constexpr (toml::is_table<decltype(el)>) {
                    auto value = el["apikey"].value<std::string>();
                    if (value.has_value()) {
                        translationAPI.apikey = *value;
                    }
                    else {
                        return;
                    }
                    value = el["apiurl"].value<std::string>();
                    if (value.has_value()) {
                        translationAPI.apiurl = cvt2StdApiUrl(value.value());
                    }
                    else {
                        return;
                    }
                    value = el["modelName"].value<std::string>();
                    if (value.has_value()) {
                        translationAPI.modelName = *value;
                    }
                    else if (transEngine != TransEngine::Sakura) {
                        return;
                    }
                    translationAPI.lastReportTime = std::chrono::steady_clock::now();
                    m_translationAPIs.push_back(translationAPI);
                }
            });

        if (m_translationAPIs.empty() && transEngine != TransEngine::DumpName && transEngine != TransEngine::Rebuild) {
            throw std::invalid_argument("找不到可用的 apikey ");
        }
        else {
            m_apiPool.loadAPIs(m_translationAPIs);
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
                        m_retranslKeys.push_back(*el);
                    }
                });
            std::erase_if(m_retranslKeys, [](const std::string& key) { return key.empty(); });
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
                            dictPath = defaultDictFolderPath / ascii2Wide(*el);
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
                            dictPath = defaultDictFolderPath / ascii2Wide(*el);
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
                            dictPath = defaultDictFolderPath / ascii2Wide(*el);
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

        if (transEngine == TransEngine::DumpName || transEngine == TransEngine::Rebuild) {
            // 这两个不需要加载提示词
            return;
        }

        // 加载提示词
        fs::path promptPath = m_projectDir / L"Prompt.toml";
        if (!fs::exists(promptPath)) {
            promptPath = L"Prompt.toml";
            if (!fs::exists(promptPath)) {
                throw std::runtime_error("找不到 Prompt.toml 文件");
            }
        }
        ifs.open(promptPath);
        auto promptData = toml::parse(ifs);
        ifs.close();

        std::string systemKey;
        std::string userKey;


        switch (transEngine) {
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


        auto value = promptData[systemKey].value<std::string>();
        if (value.has_value()) {
            m_systemPrompt = *value;
        }
        else {
            throw std::invalid_argument(std::format("Prompt.toml 中缺少 {} 键", systemKey));
        }
        value = promptData[userKey].value<std::string>();
        if (value.has_value()) {
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

/**
* @brief 根据句子的上下文生成唯一的缓存键，复刻 GalTransl 逻辑
*/
std::string NormalJsonTranslator::generateCacheKey(const Sentence* s) {
    std::string prev_text = "None";
    const Sentence* temp = s->prev;
    if (temp) {
        prev_text = temp->name + temp->pre_processed_text;
    }

    std::string current_text = s->name + s->pre_processed_text;

    std::string next_text = "None";
    temp = s->next;
    if (temp) {
        next_text = temp->name + temp->pre_processed_text;
    }

    return prev_text + current_text + next_text;
}

/**
* @brief 构建用于 Prompt 的上下文历史
*/
std::string NormalJsonTranslator::buildContextHistory(const std::vector<Sentence*>& batch) {
    if (batch.empty() || !batch[0]->prev) {
        return {};
    }

    std::string history;

    switch (m_transEngine) {
    case TransEngine::ForGalTsv: {
        history = "NAME\tDST\tID\n"; // Or DST\tID for novel
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < m_contextHistorySize) {
            if (current->complete) {
                std::string name = current->name.empty() ? "null" : current->name;
                contextLines.push_back(name + "\t" + current->pre_translated_text + "\t" + std::to_string(current->index));
                count++;
            }
            current = current->prev;
        }
        std::reverse(contextLines.begin(), contextLines.end());
        if (contextLines.empty()) return {};
        for (const auto& line : contextLines) {
            history += line + "\n";
        }
        break;
    }
    case TransEngine::ForNovelTsv: {
        history = "DST\tID\n"; // Or DST\tID for novel
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < m_contextHistorySize) {
            if (current->complete) {
                contextLines.push_back(current->pre_translated_text + "\t" + std::to_string(current->index));
                count++;
            }
            current = current->prev;
        }
        std::reverse(contextLines.begin(), contextLines.end());
        if (contextLines.empty()) return {};
        for (const auto& line : contextLines) {
            history += line + "\n";
        }
        break;
    }

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        json historyJson = json::array();
        const Sentence* current = batch[0]->prev;
        int count = 0;
        while (current && count < m_contextHistorySize) {
            if (current->complete) { // current->complete
                json item;
                item["id"] = current->index;
                if (!current->name.empty()) item["name"] = current->name;
                item["dst"] = current->pre_translated_text;
                historyJson.push_back(item);
                count++;
            }
            current = current->prev;
        }
        std::reverse(historyJson.begin(), historyJson.end());
        for (const auto& item : historyJson) {
            history += item.dump() + "\n";
        }
        history = "```jsonline\n" + history + "```";
    }
    break;

    case TransEngine::Sakura:
    {
        const Sentence* current = batch[0]->prev;
        int count = 0;
        std::vector<std::string> contextLines;
        while (current && count < m_contextHistorySize) {
            if (current->complete) {
                if (!current->name.empty()) {
                    contextLines.push_back(current->name + ":::::" + current->pre_translated_text); // :::::
                }
                else {
                    contextLines.push_back(current->pre_translated_text);
                }
                count++;
            }
            current = current->prev;
        }
        std::reverse(contextLines.begin(), contextLines.end());
        for (const auto& line : contextLines) {
            history += line + "\n";
        }
    }
    break;
    default:
        throw std::runtime_error("未知的 PromptType");
    }

    return history;
}

void NormalJsonTranslator::preProcess(Sentence* se) {
    if (m_usePreDictInName) {
        se->name = m_preDictionary.doReplace(se, ConditionTarget::Name);
    }
    if (m_usePreDictInMsg) {
        se->pre_processed_text = m_preDictionary.doReplace(se, ConditionTarget::OrigText);
    }
    else {
        se->pre_processed_text = se->original_text;
    }
    
    replaceStrInplace(se->pre_processed_text, "\t", "[t]");
    if (m_linebreakSymbol == "auto") {
        if (se->pre_processed_text.find("<br>") != std::string::npos) {
            se->originalLinebreak.clear();
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
    }
    else {
        se->originalLinebreak = m_linebreakSymbol;
    }

    if (!se->originalLinebreak.empty()) {
        replaceStrInplace(se->pre_processed_text, se->originalLinebreak, "<br>");
    }
}

void NormalJsonTranslator::postProcess(Sentence* se) {

    if (!se->originalLinebreak.empty()) {
        replaceStrInplace(se->pre_translated_text, "<br>", se->originalLinebreak);
    }
    replaceStrInplace(se->pre_translated_text, "[t]", "\t");

    for (auto& plugin : m_postPlugins) {
        plugin->run(se);
    }

    if (m_usePostDictInMsg) {
        se->translated_preview = m_postDictionary.doReplace(se, ConditionTarget::PretransText);
    }
    else {
        se->translated_preview = se->pre_translated_text;
    }

    m_problemAnalyzer.analyze(se, m_gptDictionary, m_targetLang);
}

ApiResponse NormalJsonTranslator::performApiRequest(const json& payload, const TranslationAPI& api, int threadId) {
    ApiResponse apiResponse;

    cpr::Response response = cpr::Post(
        cpr::Url{ api.apiurl },
        cpr::Body{ payload.dump() },
        cpr::Header{ {"Content-Type", "application/json"}, {"Authorization", "Bearer " + api.apikey} },
        cpr::Timeout{ m_apiTimeOutMs }
    );

    apiResponse.statusCode = response.status_code;
    apiResponse.content = response.text; // 无论成功失败，都记录下响应体

    if (response.status_code == 200) {
        try {
            apiResponse.content = json::parse(response.text)["choices"][0]["message"]["content"];
            apiResponse.success = true;
        }
        catch (const json::exception& e) {
            m_logger->error("[线程 {}] 成功响应但JSON解析失败: {}, 错误: {}", threadId, response.text, e.what());
            apiResponse.success = false;
        }
    }
    else {
        m_logger->error("[线程 {}] API 请求失败，状态码: {}, 错误: {}", threadId, response.status_code, response.text);
        apiResponse.success = false;
    }

    return apiResponse;
}

bool NormalJsonTranslator::translateBatchWithRetry(std::vector<Sentence*>& batch, int threadId) {
    if (batch.empty()) return true;

    int retryCount = 0;
    std::string contextHistory = buildContextHistory(batch);
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
            m_logger->warn("[线程 {}] 开始拆分批次进行重试...", threadId);

            size_t mid = batchToTransThisRound.size() / 2;
            std::vector<Sentence*> firstHalf(batchToTransThisRound.begin(), batchToTransThisRound.begin() + mid);
            std::vector<Sentence*> secondHalf(batchToTransThisRound.begin() + mid, batchToTransThisRound.end());

            bool firstOk = translateBatchWithRetry(firstHalf, threadId);
            bool secondOk = translateBatchWithRetry(secondHalf, threadId);

            return firstOk && secondOk;
        }
        else if (m_smartRetry && retryCount == 3) {
            m_logger->warn("[线程 {}] 清空上下文后再次尝试...", threadId);
            contextHistory.clear();
        }

        std::string inputBlock;
        std::string inputProblems;
        std::map<int, Sentence*> id2SentenceMap; // 用于 TSV/JSON 

        for (auto pSentence : batchToTransThisRound) {
            if (!pSentence->problem.empty() && inputProblems.find(pSentence->problem) == std::string::npos) {
                inputProblems += pSentence->problem + "\n";
            }
        }
        switch (m_transEngine) {
        case TransEngine::ForGalTsv: {
            for (const auto& pSentence : batchToTransThisRound) {
                std::string name = pSentence->name.empty() ? "null" : pSentence->name;
                inputBlock += name + "\t" + pSentence->pre_processed_text + "\t" + std::to_string(pSentence->index) + "\n";
                id2SentenceMap[pSentence->index] = pSentence;
            }
            break;
        }
        case TransEngine::ForNovelTsv: {
            for (const auto& pSentence : batchToTransThisRound) {
                inputBlock += pSentence->pre_processed_text + "\t" + std::to_string(pSentence->index) + "\n";
                id2SentenceMap[pSentence->index] = pSentence;
            }
            break;
        }
            
        case TransEngine::ForGalJson:
        case TransEngine::DeepseekJson:
            for (const auto& pSentence : batchToTransThisRound) {
                json item;
                item["id"] = pSentence->index;
                if (!pSentence->name.empty()) item["name"] = pSentence->name;
                item["src"] = pSentence->pre_processed_text;
                inputBlock += item.dump() + "\n";
                id2SentenceMap[pSentence->index] = pSentence;
            }
            break;

        case TransEngine::Sakura:
            for (const auto& pSentence : batchToTransThisRound) {
                if (!pSentence->name.empty()) {
                    inputBlock += pSentence->name + ":::::" + pSentence->pre_processed_text + "\n";
                }
                else {
                    inputBlock += pSentence->pre_processed_text + "\n";
                }
            }
            inputBlock.pop_back(); // 移除最后一个换行符
            break;
        default:
            throw std::runtime_error("不支持的 TransEngine 用于构建输入");
        }

        m_logger->info("[线程 {}] 开始翻译\nProblems:\n{}\nDict:\n{}\ninputBlock:\n{}", threadId, inputProblems, glossary, inputBlock);
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

        ApiResponse response = performApiRequest(payload, currentAPI, threadId);
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
                m_logger->error("[线程 {}] API Key [{}] 没有 [{}] 模型，将从池中移除。", threadId, currentAPI.apikey, currentAPI.modelName);
                m_apiPool.reportProblem(currentAPI);
                continue;
            }

            // 情况二：频率限制 (429) 或其他可重试错误
            // 状态码 429 是最明确的信号
            if (response.statusCode == 429 || lowerErrorMsg.find("rate limit") != std::string::npos || lowerErrorMsg.find("try again") != std::string::npos) {
                retryCount++;
                m_logger->warn("[线程 {}] 遇到频率限制或可重试错误，进行第 {} 次退避等待...", threadId, retryCount);

                // 实现指数退避与抖动
                int sleep_seconds;
                int max_sleep_seconds = static_cast<int>(std::pow(2, std::min(retryCount, 6)));
                if (max_sleep_seconds > 0) {
                    sleep_seconds = std::rand() % max_sleep_seconds;
                }
                else {
                    sleep_seconds = 0;
                }
                m_logger->info("[线程 {}] 将等待 {} 秒后重试...", threadId, sleep_seconds);
                if (sleep_seconds > 0) {
                    std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));
                }
                continue;
            }

            // 其他无法识别的硬性错误
            retryCount++;
            m_logger->warn("[线程 {}] 遇到未知API错误，进行第 {} 次重试...", threadId, retryCount);
            if (m_apiStrategy == "fallback") {
                m_logger->warn("[线程 {}] 将切换到下一个 API Key(如果有多个API Key的话)", threadId);
                m_apiPool.resortTokens();
            }
            std::this_thread::sleep_for(std::chrono::seconds(2)); // 简单等待
            continue;
        }

        // --- 如果请求成功，则继续解析 ---
        std::string content = response.content;
        m_logger->info("[线程 {}] 成功响应，解析结果: \n{}", threadId, content);
        int parsedCount = 0;
        bool parseError = false;

        if (content.find("</think>") != std::string::npos) {
            content = content.substr(content.find("</think>") + 8);
        }
        switch (m_transEngine) {
        case TransEngine::ForGalTsv: {
            size_t start = content.find("NAME\tDST\tID");
            if (start == std::string::npos) { 
                parseError = true; 
                break;
            }

            std::stringstream ss(content.substr(start));
            std::string line;
            std::getline(ss, line); // Skip header

            while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
                if (line.empty() || line.find("```") != std::string::npos) continue;
                auto parts = splitString(line, '\t');
                if (parts.size() < 3) { 
                    parseError = true;
                    break;
                }
                try {
                    int id = std::stoi(parts[2]);
                    if (id2SentenceMap.count(id)) {
                        id2SentenceMap[id]->pre_translated_text = parts[1];
                        id2SentenceMap[id]->translated_by = currentAPI.modelName;
                        id2SentenceMap[id]->problem = "";
                        id2SentenceMap[id]->complete = true;
                        m_completedSentences++;
                        m_controller->updateBar(); // ForGalTsv
                        parsedCount++;
                    }
                }
                catch (...) { 
                    parseError = true;
                    break; 
                }
            }
            
        }
        break;

        case TransEngine::ForNovelTsv:
        {
            size_t start = content.find("DST\tID"); // or DST\tID
            if (start == std::string::npos) { 
                parseError = true; 
                break;
            }

            std::stringstream ss(content.substr(start));
            std::string line;
            std::getline(ss, line); // Skip header

            while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
                if (line.empty() || line.find("```") != std::string::npos) continue;
                auto parts = splitString(line, '\t');
                if (parts.size() < 2) { 
                    parseError = true; 
                    break;
                }
                try {
                    int id = std::stoi(parts[1]);
                    if (id2SentenceMap.count(id)) {
                        id2SentenceMap[id]->pre_translated_text = parts[0];
                        id2SentenceMap[id]->translated_by = currentAPI.modelName;
                        id2SentenceMap[id]->problem = "";
                        id2SentenceMap[id]->complete = true;
                        m_completedSentences++;
                        m_controller->updateBar(); // ForNovelTsv
                        parsedCount++;
                    }
                }
                catch (...) { 
                    parseError = true;
                    break;
                }
            }
        }
        break;

        case TransEngine::ForGalJson:
        case TransEngine::DeepseekJson:
        {
            size_t start = std::min(content.find("{\"id\""), content.find("{\"dst\""));
            if (start == std::string::npos) { 
                parseError = true; 
                break;
            }

            std::stringstream ss(content.substr(start));
            std::string line;
            while (parsedCount < batchToTransThisRound.size() && std::getline(ss, line)) {
                if (line.empty() || !line.starts_with('{')) continue;
                try {
                    json item = json::parse(line);
                    int id = item.at("id");
                    if (id2SentenceMap.count(id)) {
                        id2SentenceMap[id]->pre_translated_text = item.at("dst");
                        id2SentenceMap[id]->translated_by = currentAPI.modelName;
                        id2SentenceMap[id]->problem = "";
                        id2SentenceMap[id]->complete = true;
                        m_completedSentences++;
                        m_controller->updateBar(); // ForGalJson/DeepseekJson
                        parsedCount++;
                    }
                }
                catch (...) { 
                    parseError = true; 
                    break;
                }
            }
        }
        break;

        case TransEngine::Sakura:
        {
            auto lines = splitString(content, '\n');
            // 核心检查：行数是否匹配
            if (lines.size() != batchToTransThisRound.size()) { 
                parseError = true; 
                break;
            }

            for (size_t i = 0; i < lines.size(); ++i) {
                std::string translatedLine = lines[i];
                Sentence* currentSentence = batchToTransThisRound[i];

                // 尝试剥离说话人
                if (!currentSentence->name.empty() && translatedLine.find(":::::") != std::string::npos) {
                    size_t msgStart = translatedLine.find(":::::");
                    translatedLine = translatedLine.substr(msgStart + 5);
                }

                currentSentence->pre_translated_text = translatedLine;
                currentSentence->translated_by = currentAPI.modelName;
                currentSentence->problem = "";
                currentSentence->complete = true;
                m_completedSentences++;
                m_controller->updateBar(); // Sakura
                parsedCount++;
            }
        }
        break;
        default:
            throw std::runtime_error("不支持的 TransEngine 用于解析输出");
        }

        if (parseError || parsedCount != batchToTransThisRound.size()) {
            retryCount++;
            m_logger->warn("[线程 {}] 解析失败或不完整 ({} / {}), 进行第 {} 次重试...", threadId, parsedCount, batchToTransThisRound.size(), retryCount);
            continue;
        }

        m_logger->debug("[线程 {}] 批次翻译成功，解析了 {} 句话。", threadId, parsedCount);
        return true;
    }

    m_logger->error("[线程 {}] 批次翻译在 {} 次重试后彻底失败。", threadId, m_maxRetries);
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

void NormalJsonTranslator::processFile(const fs::path& inputPath, int threadId) {
    m_logger->info("[线程 {}] 开始处理文件: {}", threadId, wide2Ascii(inputPath));
    m_controller->addThreadNum();

    std::ifstream ifs;
    fs::path relInputPath = fs::relative(inputPath, m_needsCombining ? m_inputCacheDir : m_inputDir);
    fs::path outputPath = m_needsCombining ? (m_outputCacheDir / relInputPath) : (m_outputDir / relInputPath);
    fs::path cachePath = m_cacheDir / relInputPath;
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
            se.name = item.value("name", "");
            se.original_text = item.value("message", "");
            preProcess(&se);
            sentences.push_back(se);
        }
        for (size_t i = 0; i < sentences.size(); ++i) {
            if (i > 0) sentences[i].prev = &sentences[i - 1];
            if (i < sentences.size() - 1) sentences[i].next = &sentences[i + 1];
        }
    }
    catch (const json::exception& e) {
        throw std::runtime_error(std::format("[线程 {}] 文件 {} 解析失败: {}", threadId, wide2Ascii(inputPath), e.what()));
    }

    std::multimap<std::string, json> cacheMap;

    {
        std::vector<fs::path> cachePaths;
        if (m_needsCombining) {
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
        }
        else if (fs::exists(cachePath)) {
            cachePaths.push_back(cachePath);
        }
        std::ranges::sort(cachePaths);

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

        for (size_t i = 0; i < totalCacheJsonList.size(); ++i) {
            const auto& item = totalCacheJsonList[i];
            std::string prevText = "None", currentText, nextText = "None";
            currentText = item.value("name", "") + item.value("pre_processed_text", "");
            if (i > 0) prevText = totalCacheJsonList[i - 1].value("name", "") + totalCacheJsonList[i - 1].value("pre_processed_text", "");
            if (i < totalCacheJsonList.size() - 1) nextText = totalCacheJsonList[i + 1].value("name", "") + totalCacheJsonList[i + 1].value("pre_processed_text", "");
            cacheMap.insert(std::make_pair(prevText + currentText + nextText, item));
        }
    }

    std::vector<Sentence*> toTranslate;
    for (auto& se : sentences) {
        if (se.original_text.empty()) continue;
        std::string key = generateCacheKey(&se);
        auto it = findSame(cacheMap, key, &se);
        if (m_transEngine == TransEngine::Rebuild) {
            if (it != cacheMap.end()) {
                const auto& item = it->second;
                se.pre_translated_text = item.value("pre_translated_text", "");
                se.translated_by = item.value("translated_by", "");
                se.complete = true;
                m_completedSentences++;
                m_controller->updateBar(); // 命中缓存
                postProcess(&se);
                continue;
            }
            else {
                m_logger->error("[线程 {}] 缓存中找不到句子: [{}]，可能是修改了译前词典或句子尚未缓存", threadId, se.original_text);
                m_rebuildSuccess = false;
                se.pre_translated_text = se.pre_processed_text;
                se.complete = true;
                m_completedSentences++;
                m_controller->updateBar(); // 未命中缓存
                postProcess(&se);
                continue;
            }
        }
        if (it == cacheMap.end()) {
            toTranslate.push_back(&se);
            continue;
        }
        const auto& item = it->second;
        se.problem = item.value("problem", "");
        if (!hasRetranslKey(m_retranslKeys, &se)) {
            se.pre_translated_text = item.value("pre_translated_text", "");
            se.translated_by = item.value("translated_by", "");
            se.complete = true;
            m_completedSentences++;
            m_controller->updateBar(); // 命中缓存
            postProcess(&se);
            continue;
        }
        else {
            toTranslate.push_back(&se);
        }
    }
    m_logger->info("[线程 {}] 文件 {} 共 {} 句，命中缓存 {} 句，需翻译 {} 句。", threadId, wide2Ascii(inputPath.filename()),
        sentences.size(), sentences.size() - toTranslate.size(), toTranslate.size());

    int batchCount = 0;
    for (size_t i = 0; i < toTranslate.size(); i += m_batchSize) {
        if (m_controller->shouldStop()) {
            return;
        }
        std::vector<Sentence*> batch(toTranslate.begin() + i, toTranslate.begin() + std::min(i + m_batchSize, toTranslate.size()));
        translateBatchWithRetry(batch, threadId);
        for (auto& se : batch) {
            postProcess(se);
        }
        batchCount++;
        if (batchCount % m_saveCacheInterval == 0) {
            m_logger->debug("[线程 {}] 文件 {} 达到保存间隔，正在更新缓存文件...", threadId, wide2Ascii(inputPath));
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            saveCache(sentences, cachePath);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_logger->debug("[线程 {}] 文件 {} 翻译完成，正在进行最终保存...", threadId, wide2Ascii(inputPath.filename()));
        saveCache(sentences, cachePath);
    }

    json outputJson = json::array();
    for (auto& se : sentences) {
        json item;
        if (!se.name.empty()) {
            if (m_useGPTDictToReplaceName) {
                se.name = m_gptDictionary.doReplace(&se, ConditionTarget::Name);
            }
            auto it = m_nameMap.find(se.name);
            if (it != m_nameMap.end() && !it->second.empty()) {
                se.name = it->second;
            }
            if (m_usePostDictInName) {
                se.name = m_postDictionary.doReplace(&se, ConditionTarget::Name);
            }
            item["name"] = se.name;
        }
        item["message"] = se.translated_preview;
        if (m_outputWithSrc) {
            item["src_msg"] = se.original_text;
        }
        outputJson.push_back(item);
    }

    std::lock_guard<std::mutex> lock(m_outputCacheFileMutex);
    std::ofstream ofs(outputPath);
    ofs << outputJson.dump(2);
    ofs.close();

    m_logger->info("[线程 {}] 文件 {} 处理完成。", threadId, wide2Ascii(inputPath));
    m_controller->reduceThreadNum();

    if (m_needsCombining) {
        size_t pos = relInputPath.wstring().rfind(L"_part_");
        if (pos == std::string::npos) {
            throw std::runtime_error("文件名不符合要求，无法尝试合并。");
        }
        fs::path originalRelFilePath = relInputPath.wstring().substr(0, pos) + L".json";
        auto it = m_splitFilePartsCount.find(wstr2Lower(originalRelFilePath));
        if (it == m_splitFilePartsCount.end()) {
            throw std::runtime_error(std::format("splitFilePartsCount 中找不到文件名 {}", wide2Ascii(originalRelFilePath)));
        }
        int partCount = it->second;
        if (!isAllPartFilesReady(originalRelFilePath, partCount, m_outputCacheDir)) {
            m_logger->debug("文件 {} 尚未全部处理完成，跳过合并。", wide2Ascii(originalRelFilePath));
            return;
        }
        m_logger->debug("开始合并 {} 的翻译文件...", wide2Ascii(originalRelFilePath));
        combineOutputFiles(originalRelFilePath, partCount, m_outputCacheDir, m_outputDir);
    }
}

/**
 * @brief 合并由 processFile 生成的部分输出文件
 * @param originalRelFilePath 原始输入文件的相对input路径
 * @param numParts 分割的总份数
 * @param outputDir 输出目录
 */
void NormalJsonTranslator::combineOutputFiles(const fs::path& originalRelFilePath, int numParts, const fs::path& outputCacheDir, const fs::path& outputDir) {
    json combinedJson = json::array();
    std::wstring stem = originalRelFilePath.parent_path() / originalRelFilePath.stem();

    std::ifstream ifs;
    m_logger->debug("开始合并文件: {}", wide2Ascii(originalRelFilePath));

    for (int i = 0; i < numParts; ++i) {
        fs::path partPath = outputCacheDir / (stem + L"_part_" + std::to_wstring(i) + L".json");
        if (fs::exists(partPath)) {
            try {
                ifs.open(partPath);
                json partData = json::parse(ifs);
                ifs.close();
                fs::remove(partPath);
                if (partData.is_array()) {
                    combinedJson.insert(combinedJson.end(), partData.begin(), partData.end());
                }
            }
            catch (const json::exception& e) {
                ifs.close();
                m_logger->critical("合并文件 {} 时出错", wide2Ascii(partPath));
                throw std::runtime_error(e.what());
            }
        }
        else {
            throw std::runtime_error(std::format("试图合并 {} 时出错，缺少文件 {}", wide2Ascii(originalRelFilePath), wide2Ascii(partPath)));
        }
    }

    fs::path finalOutputPath = outputDir / originalRelFilePath;
    createParent(finalOutputPath);
    try {
        std::ofstream ofs(finalOutputPath);
        ofs << combinedJson.dump(2);
        m_logger->info("文件 {} 合并完成，已保存到 {}", wide2Ascii(originalRelFilePath), wide2Ascii(finalOutputPath));
    }
    catch (const std::exception& e) {
        m_logger->critical("保存最终合并文件 {} 失败", wide2Ascii(finalOutputPath));
        throw std::runtime_error(e.what());
    }
}

bool NormalJsonTranslator::isAllPartFilesReady(const fs::path& originalRelFilePath, int numParts, const fs::path& outputCacheDir) {
    std::wstring stem = originalRelFilePath.parent_path() / originalRelFilePath.stem();
    for (int i = 0; i < numParts; ++i) {
        fs::path partPath = outputCacheDir / (stem + L"_part_" + std::to_wstring(i) + L".json");
        if (!fs::exists(partPath)) {
            m_logger->debug("文件尚不存在: {}", wide2Ascii(partPath));
            return false;
        }
    }
    return true;
}

void NormalJsonTranslator::run() {
    m_logger->info("GalTranslPP NormalJsonTranlator 启动...");

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
                    se.name = m_preDictionary.doReplace(&se, ConditionTarget::Name);
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
            return;
        }
    }

    if (m_transEngine == TransEngine::GenDict) {
        DictionaryGenerator generator(m_controller, m_apiPool, ascii2Wide(m_dictDir), m_systemPrompt, m_userPrompt, m_threadsNum, m_apiTimeOutMs);
        generator.setLogger(m_logger);
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
            std::string transName = value.as_array()->get(0)->value_or("");
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

                std::vector<json> parts = splitJsonArray(data, m_splitFileNum);
                m_splitFilePartsCount[wstr2Lower(fs::relative(entry.path(), m_inputDir))] = (int)parts.size();

                std::wstring stem = fs::relative(entry.path(), m_inputDir).parent_path() / entry.path().stem();
                for (size_t i = 0; i < parts.size(); ++i) {
                    fs::path partPath = m_inputCacheDir / (stem + L"_part_" + std::to_wstring(i) + L".json");
                    createParent(partPath);
                    ofs.open(partPath);
                    ofs << parts[i].dump(2);
                    ofs.close();
                }
                m_logger->debug("文件 {} 已被分割成 {} 份，存入输入缓存。", wide2Ascii(entry.path().filename()), parts.size());

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

                std::vector<json> parts = splitJsonArrayByChunkSize(data, m_splitFileNum);
                m_splitFilePartsCount[wstr2Lower(fs::relative(entry.path(), m_inputDir))] = (int)parts.size(); // 记录份数

                std::wstring stem = fs::relative(entry.path(), m_inputDir).parent_path() / entry.path().stem();
                for (size_t i = 0; i < parts.size(); ++i) {
                    fs::path partPath = m_inputCacheDir / (stem + L"_part_" + std::to_wstring(i) + L".json");
                    createParent(partPath);
                    ofs.open(partPath);
                    ofs << parts[i].dump(2);
                    ofs.close();
                }
                m_logger->debug("文件 {} 已被按每 {} 句分割成 {} 份，存入输入缓存。",
                    wide2Ascii(entry.path().filename()), m_splitFileNum, parts.size());

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


    m_logger->debug("开始从目录 {} 分发翻译任务...", wide2Ascii(sourceDir.wstring()));
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
    m_controller->makeBar(m_totalSentences, m_threadsNum);

    for (const auto& filePath : filePaths) {
        results.emplace_back(pool.push([=](int id) {
            this->processFile(filePath, id);
            }));
    }

    m_logger->info("已将 {} 个文件任务分配到线程池，等待处理完成...", results.size());

    for (auto& result : results) {
        result.get();
    }

    fs::remove_all(m_inputCacheDir);
    fs::remove_all(m_outputCacheDir);
    if (m_transEngine == TransEngine::Rebuild && !m_rebuildSuccess) {
        m_logger->critical("重建失败，请检查log以定位问题");
    }
    else {
        m_logger->info("所有任务已完成！NormalJsonTranlator结束。");
    }
}

