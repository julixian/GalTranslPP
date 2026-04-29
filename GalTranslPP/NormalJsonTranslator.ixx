module;

#define PYBIND11_HEADERS
#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <ctpl_stl.h>
#include <proxy/proxy.h>
#include <toml.hpp>

export module NormalJsonTranslator;

import APIPool;
import AgentSourceView;
import Dictionary;
import IPlugin;
import GPPDefines;
import ProblemAnalyzer;
import LuaManager;
import PythonManager;
import ITranslator;

namespace fs = std::filesystem;

export {

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
        std::string rollingContext;
    };

    class NormalJsonTranslator : public ITranslator {

        friend void pybind11_init_gpp_plugin_api(::pybind11::module_& m);
        friend class LuaManager;

    protected:
        TransEngine m_transEngine{};
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;

        fs::path m_inputDir;
        fs::path m_inputCacheDir;
        fs::path m_outputDir;
        fs::path m_outputCacheDir;
        fs::path m_transCacheDir;
        fs::path m_otherCacheDir;
        fs::path m_backgroundTextCachePath;
        fs::path m_projectDir;
        fs::path m_agentRootDir;
        fs::path m_agentRunStatePath;
        fs::path m_agentTermLedgerPath;
        fs::path m_agentRewriteQueuePath;
        fs::path m_agentTermConflictPath;
        fs::path m_agentFileNotesDir;
        fs::path m_agentSearchCatalogPath;

        // 键为相对路径字符串，值为该文件对应的背景摘要文本
        std::shared_mutex m_backgroundTextCacheMapMutex;
        absl::flat_hash_map<std::string, std::string> m_backgroundTextCacheMap;

        std::string m_systemPrompt;
        std::string m_userPrompt;
        std::string m_agentSystemPrompt;
        std::string m_agentUserPrompt;
        std::string m_genDictReviewSystemPrompt;
        std::string m_genDictReviewUserPrompt;
        std::string m_targetLang;

        bool m_pythonTranslator = false;

        int m_threadsNum{};
        int m_batchSize{};
        int m_contextHistorySize{};
        int m_maxRetries{};
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
        // 仅在最终 reconcile 阶段为 true。
        bool m_agentReconciling = false;
        int m_lastRuntimeFileTotal = 0;

        std::string m_apiStrategy;
        std::string m_sortMethod;
        std::string m_splitFile;
        int m_splitFileNum{};
        int m_cacheSearchDistance{};
        std::string m_linebreakSymbol;
        std::string m_agentRewriteMode;
        int m_agentMaxTurnsPerChunk{};
        int m_agentSoftContextChars{};
        int m_agentHardContextChars{};
        int m_agentSearchResultLimit{};
        std::vector<CheckSeCondFunc> m_retranslKeys;
        // first: 要忽略的问题正则表达式，second: 对应的忽略条件
        using SkipProblemCondition = std::pair<jpc::Regex, std::optional<CheckSeCondFunc>>;
        std::vector<SkipProblemCondition> m_skipProblems;

        std::unique_ptr<LuaManager> m_luaManager;
        std::unique_ptr<PythonManager> m_pythonManager;

        bool m_needsCombining = false; // 是否开启单文件分割
        std::shared_mutex m_transCacheMutex;
        std::mutex m_outputMutex;
        std::mutex m_agentStateMutex;
        std::mutex m_agentFileNotesMutex;
        json m_agentRunStateCache = json::object();
        json m_agentTermLedgerCache = json::object();
        json m_agentRewriteQueueCache = json::array();
        json m_agentTermConflictCache = json::array();
        absl::flat_hash_map<fs::path, json> m_agentFileNoteCache;
        std::shared_mutex m_agentLoadedDictionaryEntriesCacheMutex;
        std::shared_ptr<const json> m_agentLoadedDictionaryEntriesCache;
        // 输入分割文件相对路径到原始json相对路径的映射
        absl::flat_hash_map<fs::path, fs::path> m_splitFilePartsToJson;
        // 原始json相对路径到多个输入分割文件相对路径及其有没有完成的映射
        absl::flat_hash_map<fs::path, absl::flat_hash_map<fs::path, bool>> m_jsonToSplitFileParts;
        // 最终 reconcile 阶段要求强制重翻的 file/id 白名单。
        // 这里不直接改写 trans_cache 文件，否则会破坏基于前后句生成的 cache key，
        // 导致邻近句子也误判为 cache miss。
        absl::flat_hash_map<fs::path, absl::flat_hash_set<int>> m_agentReconcileTargetsByFile;
        std::vector<fs::path> m_agentKnownRelFiles;
        std::vector<fs::path> m_agentDictionaryPaths;
        std::optional<fs::path> m_agentProjectInfoPath;
        absl::flat_hash_map<fs::path, AgentSourceFileView> m_agentSourceFileViews;

        absl::flat_hash_map<std::string, std::string> m_nameMap;
        toml::ordered_value m_problemOverview = toml::array{};
        std::function<void(fs::path)> m_onFileProcessed;
        std::function<std::string(std::string)> m_onPerformApi;
        std::function<DictList(DictList)> m_onDictProcessed;

        ctpl::thread_pool m_threadPool{1};
        std::unique_ptr<APIPool> m_apiPool;
        std::unique_ptr<GptDictionary> m_gptDictionary;
        std::unique_ptr<NormalDictionary> m_preDictionary;
        std::unique_ptr<NormalDictionary> m_postDictionary;
        std::unique_ptr<ProblemAnalyzer> m_problemAnalyzer;
        std::function<NLPResult(const std::string&)> m_tokenizeSourceLangFunc;
        std::vector<pro::proxy<PPlugin>> m_textPlugins;


        void preProcess(Sentence* se);

        void postProcess(Sentence* se);

        bool translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId);
        // Agent 模式会在单个 chunk 内运行一个小型多轮循环。
        // 正常情况下每个 chunk 只会进入一次；如果 commit 校验失败或响应解析失败，
        // 则会按现有重试逻辑再次进入。
        bool translateBatchAgent(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText, int threadId);

	    // Agent 模式共享持久化状态的辅助函数。
	    // 所有写路径都应经过这些函数，避免 worker 线程在 load-modify-save 窗口内互相覆盖。
	    json loadAgentTermLedger();
	    json loadAgentFileNote(const fs::path& targetRelPath);
	    void saveAgentFileNote(const fs::path& targetRelPath, const json& note);
	    void mutateAgentState(const std::function<void(json& termLedger, json& rewriteQueue, json& termConflicts)>& mutator);
        std::string buildAgentLogBlock(const fs::path& relInputPath, std::span<Sentence*> batch, const std::string& rollingSummary);
        json buildAgentBaseMessages(const fs::path& relInputPath, std::span<Sentence*> batch, const std::string& rollingSummary);
        void applyAgentCommit(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& backgroundText,
            int threadId, const AgentProtocolResponse& protocol, const std::string& modelName, int& committedCount);

        void processFile(const fs::path& relInputPath, int threadId);
        bool shouldReportRuntimeWorkbench() const;
        void recordSentenceDone(const fs::path& relInputPath, const Sentence& se, bool addToSuccessStream = false) const;
        void recordRuntimeError(const std::string& kind, const std::string& message, const fs::path& relInputPath = {},
            const std::string& indexRange = {}, int retryCount = -1, const std::string& model = {},
            double sleepSeconds = -1.0, const std::string& level = "error") const;

        void runAgentFinalReconcile();

    public:
        NormalJsonTranslator(const fs::path& projectDir, const std::shared_ptr<IController>& controller, const std::shared_ptr<spdlog::logger>& logger,
            std::optional<fs::path> inputDir = std::nullopt, std::optional<fs::path> inputCacheDir = std::nullopt,
            std::optional<fs::path> outputDir = std::nullopt, std::optional<fs::path> outputCacheDir = std::nullopt);

        virtual ~NormalJsonTranslator() override;

        void normalJsonInit();
        std::optional<std::vector<fs::path>> normalJsonBeforeRun();
        void normalJsonProcess(std::vector<fs::path> relFilePaths);
        void normalJsonAfterRun();

        virtual void run() override;
    };
}
