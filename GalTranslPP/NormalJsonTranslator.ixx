module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>
#include <proxy/proxy.h>
#include <toml.hpp>

export module NormalJsonTranslator;

export import ApiPool;
export import Dictionary;
export import DictionaryGenerator;
export import IPlugin;
export import GPPDefines;
export import NameTranslator;
export import ProblemAnalyzer;
export import LuaManager;
export import PythonManager;
export import ITranslator;
export import :TransAgent;

namespace fs = std::filesystem;

export
{
    class NormalJsonTranslator : public ITranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        TransEngine m_transEngine{};
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        std::chrono::steady_clock::time_point m_startTimePoint = std::chrono::steady_clock::now();

        fs::path m_inputDir;
        fs::path m_inputCacheDir;
        fs::path m_outputDir;
        fs::path m_outputCacheDir;
        fs::path m_transCacheDir;
        fs::path m_otherCacheDir;
        fs::path m_nameTablePath;
        fs::path m_rollingContextCachePath;
        fs::path m_projectDir;
        fs::path m_agentRootDir;
        fs::path m_agentTermLedgerPath;
        fs::path m_agentFileNotesDir;

        // 键为相对路径字符串，值为该文件对应的滚动上下文
        // 键不是 fs::path 的原因主要是好读写 json
        std::shared_mutex m_rollingContextCacheMapMutex;
        absl::flat_hash_map<std::string, std::string> m_rollingContextCacheMap;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_agentSystemPrompt;
        std::string m_agentUserPrompt;
        std::string m_genDictReviewSystemPrompt;
        std::string m_genDictReviewUserPrompt;
        std::string m_targetLang;


        bool m_pythonTranslator = false;


        int m_threadsNum{};
        int m_nameTransBatchSize{};
        int m_batchSize{};
        int m_contextHistorySize{};
        int m_inputBlockMaxLines{};
        int m_problemMaxLines{};
        int m_glossaryMaxLines{};
        int m_maxRequestCount{};
        int m_saveCacheInterval{};
        int m_apiTimeOutMs{};
        bool m_checkQuota{};
        bool m_smartRetry{};
        bool m_retransAllWhenFail{};
        bool m_usePreDictInName{};
        bool m_usePostDictInName{};
        bool m_usePreDictInMsg{};
        bool m_usePostDictInMsg{};
        bool m_useGptDictToReplaceName{};
        bool m_outputWithSrc{};
        bool m_agentEnabled{};
        bool m_reuseRepeatedBlocks{};

        std::string m_apiStrategy;
        std::string m_sortMethod;
        std::string m_splitFileMethod;
        std::string m_problemOverviewFormat;
        int m_splitFileNum{};
        int m_repeatedBlockMinSize{};
        int m_cacheSearchDistance{};
        std::string m_linebreakSymbol;
        int m_agentMaxTurnsPerChunk{};
        int m_agentCompactContextThresholdBytes{};
        int m_agentSearchResultLimit{};
        int m_agentContextLinesLimit{};
        std::vector<CheckSeCondNormalFunc> m_retranslKeys;
        std::vector<SkipProblemCondition> m_skipProblems;

        std::unique_ptr<LuaManager> m_luaManager;
        std::unique_ptr<PythonManager> m_pythonManager;

        bool m_splitFileEnabled = false; // 是否开启单文件分割
        // 输入分割文件相对路径到原始json相对路径的映射
        absl::flat_hash_map<fs::path, fs::path> m_splitFilePartsToJson;
        // 原始json相对路径到多个输入分割文件相对路径及其有没有完成的映射
        absl::flat_hash_map<fs::path, absl::flat_hash_map<fs::path, bool>> m_jsonToSplitFileParts;

        std::shared_mutex m_transCacheMutex;
        std::mutex m_outputMutex;
        std::vector<fs::path> m_gptDictionaryPaths;
        std::optional<fs::path> m_agentProjectNotePath;

        absl::flat_hash_map<std::string, std::string> m_nameMap;
        std::optional<std::vector<fs::path>> m_currentRunRelFilePaths;
        absl::flat_hash_set<fs::path> m_repeatedBlockCompletedRelFilePaths;
        ordered_json m_problemOverview = ordered_json::array();
        std::function<void(fs::path)> m_onFileProcessed;
        std::function<std::string(std::string)> m_onPerformApi;
        std::function<DictList(DictList)> m_onDictProcessed;

        ctpl::thread_pool m_threadPool{1};
        std::unique_ptr<ApiPool> m_apiPool;
        std::unique_ptr<GptDictionary> m_gptDictionary;
        std::unique_ptr<NormalDictionary> m_preDictionary;
        std::unique_ptr<NormalDictionary> m_postDictionary;
        std::unique_ptr<ProblemAnalyzer> m_problemAnalyzer;
        std::unique_ptr<NameTranslator> m_nameTranslator;
        std::unique_ptr<DictionaryGenerator> m_dictionaryGenerator;
        std::unique_ptr<NormalJsonTranslatorTransAgent> m_transAgent;
        NLPTokenizeFunc m_tokenizeSourceLangFunc;
        std::vector<pro::proxy<PPlugin>> m_textPlugins;



        void recordSentenceDoneHelper(const fs::path& relInputPath, const Sentence& se, bool addToRuntimeTransSuccessStream = false) const;

        void preProcess(Sentence* se);
        void postProcess(Sentence* se);

        bool translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& rollingContext,
            int threadId, int batchIndex, int& recursionIndex, int& recursionCount);

        void processFile(const fs::path& relInputPath, int threadId);
        void normalJsonProcessFiles(const std::vector<fs::path>& relFilePaths);

        void resolveRepeatedBlockReferences();

    public:
        NormalJsonTranslator(const fs::path& projectDir,
            const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
            const std::optional<fs::path>& inputDir = std::nullopt, const std::optional<fs::path>& inputCacheDir = std::nullopt,
            const std::optional<fs::path>& outputDir = std::nullopt, const std::optional<fs::path>& outputCacheDir = std::nullopt);

        ~NormalJsonTranslator() override;

        void normalJsonInit();
        void normalJsonBeforeRun();
        void normalJsonProcess();
        void normalJsonAfterRun();

        void run() override;
    };
}
