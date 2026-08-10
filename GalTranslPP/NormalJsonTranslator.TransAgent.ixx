module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module NormalJsonTranslator:TransAgent;

export import AgentToolCommon;
export import ApiPool;
export import Dictionary;
export import ITranslator;

namespace fs = std::filesystem;

export
{
    struct TransAgentProtocolResponse {
        std::string action;
        std::vector<AgentCommonToolCallRequest> calls;
        json translations = json::array();
        json termUpdates = json::array();
        json agentSuggestions = json::array();
        json fileNotePatch = json::object();
        std::string rollingContext;
    };

    // NormalJsonTranslator 的翻译 Agent 工作流对象。
    //
    // 调用流程：
    // 1. normalJsonBeforeRun() 用 make_unique<NormalJsonTranslatorTransAgent>() 直接传入 Api 池、提示词、缓存、锁和工具限制。
    // 2. NormalJsonTranslator::processFile() 在 Agent 分支直接调用 translateBatch()。
    // 3. translateBatch() 为当前批次构造消息，驱动“模型请求 -> 工具调用/上下文压缩/提交”的多轮循环。
    // 4. applyCommit() 校验并写入句子译文、术语账本、文件备注和 Agent 建议。
    // 5. applyAgentSuggestions() 在文件处理结束后把跨文件 Agent 建议写回翻译缓存。
    //
    class NormalJsonTranslatorTransAgent {
    public:
        // 保存本轮运行依赖，并初始化翻译 Agent 自己持有的运行态。
        NormalJsonTranslatorTransAgent(
            TransEngine transEngine,
            const std::shared_ptr<IController>& controller,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::unique_ptr<ApiPool>& apiPool,
            const std::unique_ptr<GptDictionary>& gptDictionary,
            const std::function<std::string(const std::string&)>& onPerformApi,
            const fs::path& projectDir,
            const fs::path& sourceRootDir,
            const fs::path& transCacheDir,
            const fs::path& agentTermLedgerPath,
            const fs::path& agentFileNotesDir,
            const std::string& agentSystemPrompt,
            const std::string& agentUserPrompt,
            const std::string& targetLang,
            const std::string& apiStrategy,
            int maxRequestCount,
            int apiTimeOutMs,
            int agentMaxTurnsPerChunk,
            int agentCompactContextThresholdBytes,
            int agentSearchResultLimit,
            int agentContextLinesLimit,
            int inputBlockMaxLines,
            int problemMaxLines,
            int glossaryMaxLines,
            bool smartRetry,
            bool checkQuota,
            std::shared_mutex& transCacheMutex,
            const absl::flat_hash_set<fs::path>& savedTransCacheRelFilePaths,
            const std::vector<fs::path>& knownRelFiles,
            const std::vector<fs::path>& gptDictionaryPaths,
            const std::optional<fs::path>& agentProjectNotePath,
            const std::function<void(Sentence*)>& preProcessFunc
        );

        // 翻译一个 NormalJsonTranslator 批次，内部完成工具调用、压缩上下文和提交校验。
        bool translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& rollingContext,
            int threadId, int batchIndex);

        // 把提交阶段记录的跨文件 Agent 建议写入翻译缓存的 problems 字段。
        void applyAgentSuggestions();

    private:
        struct TransAgentTurnResult {
            enum class Action {
                ContinueTurn,
                CompleteBatch
            };
            Action action = Action::ContinueTurn;
            std::string summary;
        };

        struct TransAgentToolCallResult {
            json results = json::array();
            std::string summary;
            std::string detail;
        };

        struct LoadedDictionaryEntry {
            std::string sourceTerm;
            std::string targetTerm;
            std::string note;
            std::string haystackLower;
        };

        TransEngine m_transEngine{};
        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<ApiPool>& m_apiPool;
        const std::unique_ptr<GptDictionary>& m_gptDictionary;
        const std::function<std::string(const std::string&)>& m_onPerformApi;
        fs::path m_projectDir;
        fs::path m_transCacheDir;
        fs::path m_agentTermLedgerPath;
        fs::path m_agentFileNotesDir;
        std::string m_agentSystemPrompt;
        std::string m_agentUserPrompt;
        std::string m_targetLang;
        std::string m_apiStrategy;
        int m_maxRequestCount = 5;
        int m_apiTimeOutMs = 120000;
        int m_agentMaxTurnsPerChunk = 6;
        int m_agentCompactContextThresholdBytes = 0;
        int m_agentSearchResultLimit = 80;
        int m_agentContextLinesLimit = 20;
        int m_inputBlockMaxLines = 10;
        int m_problemMaxLines = 3;
        int m_glossaryMaxLines = 5;
        bool m_smartRetry = true;
        bool m_checkQuota = true;
        std::shared_mutex& m_transCacheMutex;
        const absl::flat_hash_set<fs::path>& m_savedTranslCacheRelFilePaths;
        std::mutex m_stateMutex;
        json m_termLedgerCache = json::object();
        std::mutex m_fileNotesMutex;
        absl::flat_hash_map<fs::path, json> m_fileNoteCache;
        absl::flat_hash_map<fs::path, absl::flat_hash_map<int, std::vector<std::string>>> m_agentSuggestions;
        std::shared_mutex m_loadedDictionaryEntriesCacheMutex;
        std::shared_ptr<const json> m_loadedDictionaryEntriesCache;
        std::vector<fs::path> m_knownRelFiles;
        std::vector<fs::path> m_gptDictionaryPaths;
        std::optional<fs::path> m_agentProjectNotePath;
        absl::flat_hash_map<fs::path, AgentCommonSourceFileView> m_sourceFileViews;

        // 在线程锁下读取共享术语账本快照。
        json loadTermLedger();

        // 读取指定文件备注，优先使用内存缓存，首次访问时从磁盘加载。
        json loadFileNote(const fs::path& targetRelPath);

        // 解析并校验翻译 Agent 文本协议，得到动作、工具调用和提交字段。
        TransAgentProtocolResponse parseProtocolResponse(const std::string& content) const;

        // 把 Agent 建议目标解析为相对文件和句子 id。
        std::optional<std::pair<fs::path, int>> parseAgentSuggestTarget(const json& suggestion) const;

        // 读取已配置的翻译字典，并构造成小写搜索缓存项。
        std::vector<LoadedDictionaryEntry> loadDictionaryEntries() const;

        // 收集当前批次里还没有完成翻译的句子。
        std::vector<Sentence*> collectPendingSentences(std::span<Sentence*> batch) const;

        // 根据当前 chunk、滚动上下文和文件备注构造翻译 Agent 可读的术语账本摘要。
        std::string buildTermLedgerExcerpt(
            const json& ledger,
            const std::string& currentInputBlock,
            const std::string& rollingContext,
            const json& fileNote
        ) const;

        // 汇总当前待翻译句子已有的问题文本，供提示词和日志使用。
        std::string buildProblemSummary(std::span<Sentence*> pending) const;

        // 生成当前待翻译句子的 GPT 字典提示片段。
        std::string buildGlossary(std::span<Sentence*> pending) const;

        // 合并模型提交的文件备注补丁。
        void mergeFileNotePatch(json& note, const json& patch) const;

        // 记录术语在当前文件和句子 id 上的出现位置。
        void addToTermOccurrence(json& entry, const fs::path& file, int id) const;

        // 当模型没有显式给出 line_ids 时，在当前 chunk 内推断术语出现的句子 id。
        std::vector<int> inferOccurrenceIdsFromChunk(const std::string& sourceTerm, std::span<Sentence*> pending) const;

        // 格式化术语变化引发的 Agent 建议文本。
        std::string formatTermUpdateSuggestion(const std::string& sourceTerm, const std::string& oldTarget, const std::string& newTarget) const;

        // 为工具调用查找源文件只读视图。
        const AgentCommonSourceFileView* findSourceFileView(const fs::path& relPath) const;

        // 读取源文件行数，供 list_files 输出辅助信息。
        std::optional<int> getSourceFileLineCount(const fs::path& relPath) const;

        // 读取目标文件的翻译缓存，以源句 id 为键供工具展示译文预览。
        absl::flat_hash_map<int, json> loadCacheDstMap(const fs::path& targetRelPath) const;

        // 执行 read_lines，只读取当前运行前建立的源文件视图和翻译缓存。
        json runReadLinesTool(const fs::path& relInputPath, const json& arguments);

        // 执行 search_text，在当前文件、指定文件或全部已知文件里搜索源文。
        json runSearchTextTool(const fs::path& relInputPath, const json& arguments) const;

        // 执行 search_term，在翻译 Agent 的术语账本里搜索已记录术语。
        json runSearchTermTool(const json& arguments);

        // 执行 search_dictionary，在配置的 GPT 字典里搜索候选术语。
        json runSearchDictionaryTool(const json& arguments);

        // 执行 get_file_note，读取目标文件的 Agent 笔记。
        json runGetFileNoteTool(const fs::path& relInputPath, const json& arguments);

        // 分发本轮模型请求的工具调用，并合并回填 JSON、摘要和调试明细。
        TransAgentToolCallResult executeToolCalls(
            const fs::path& relInputPath,
            const std::vector<AgentCommonToolCallRequest>& calls,
            bool collectDetail
        );

        // 解析一轮 Agent 响应，并执行工具调用、压缩上下文或提交译文。
        std::expected<TransAgentTurnResult, std::string> parseAndApplyTurnResponse(
            const fs::path& relInputPath,
            std::span<Sentence*> pending,
            std::string& rollingContext,
            json& messages,
            const std::string& content,
            const std::string& modelName,
            const std::string& batchIndexLog,
            int turn,
            int requestCount,
            int threadId
        );

        // 构造本批次开始前写入日志的可读摘要。
        std::string buildLogBlock(const fs::path& relInputPath, std::span<Sentence*> pending, const std::string& rollingContext);

        // 构造当前批次发送给模型的消息，包括术语、文件备注、滚动上下文和工具说明。
        json buildBaseMessages(const fs::path& relInputPath, std::span<Sentence*> pending, const std::string& rollingContext);

        // 校验并应用提交响应，写入译文、术语账本、文件备注和 Agent 建议。
        int applyCommit(const fs::path& relInputPath, std::span<Sentence*> pending, std::string& rollingContext,
            int threadId, const TransAgentProtocolResponse& protocol, const std::string& modelName,
            const std::string& batchIndexLog, int turn, int requestCount,
            std::string& commitResultLog, int& recordedTermUpdateCount, int& recordedSuggestionCount);
    };
}
