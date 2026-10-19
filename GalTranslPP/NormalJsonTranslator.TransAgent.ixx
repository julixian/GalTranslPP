module;

#include "GPPMacros.hpp"

export module NormalJsonTranslator:TransAgent;

export import AgentToolCommon;
export import ApiPool;
export import Dictionary;
export import ITranslator;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

// NormalJsonTranslator 的翻译 Agent 工作流对象。
//
// 调用流程：
// 1. normalJsonBeforeRun() 用 make_unique<NormalJsonTranslatorTransAgent>() 直接传入 Api 池、提示词、缓存、锁和工具限制。
// 2. NormalJsonTranslator::processFile() 在 Agent 分支直接调用 translateBatch()。
// 3. translateBatch() 为当前批次构造消息，驱动“模型请求 -> 工具调用/上下文压缩/提交”的多轮循环。
// 4. applyCommit() 校验并写入句子译文、术语账本、文件备注和 Agent 建议。
// 5. applyAgentSuggestions() 在文件处理结束后把跨文件 Agent 建议写回翻译缓存。
// 原生工具请求由 performAgentApiRequest 执行，工具结果按调用 id 回填。
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
        const std::function<std::string(std::string_view)>& onPerformApi,
        const fs::path& projectDir,
        const absl::flat_hash_map<fs::path, ordered_json>& inputJsonMap,
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
        int enhanceJailbreak,
        std::shared_mutex& transCacheMutex,
        absl::flat_hash_map<fs::path, json>& savedTranslCacheMap,
        const std::vector<fs::path>& knownRelFiles,
        const std::vector<fs::path>& gptDictionaryPaths,
        const std::optional<fs::path>& agentProjectNotePath,
        const std::function<void(Sentence*)>& preProcessFunc
    );

    // 按 worker 复用跨文件会话，驱动原生工具调用、上下文压缩和提交。
    // 智能重试先恢复批次前的会话，再对半拆分未完成句子。
    // 完成返回 true；停止或耗尽限制返回 false，耗尽时标记剩余句子失败。
    bool translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch,
        int& recursionIndex, int& recursionCount, int threadId, int batchIndex);

    // 把提交阶段记录的跨文件 Agent 建议写入缓存 other_info["gpp.agent.suggestion"]。
    void applyAgentSuggestions();

    // normalJsonBeforeRun 在构造 Agent 后调用，按 worker 数建立会话槽并生成原生工具声明。
    // 此处不发请求、不解析响应；初选不到 API 的会话槽保持为空，实际翻译时再选择。
    void configureWorkers(
        int workerCount, bool allowPython,
        const fs::path& inputDir, const fs::path& outputDir);

private:
    struct TransAgentWorker {
        std::optional<AgentApiSession> session;
        std::string rollingContext;
    };

    std::vector<TransAgentWorker> m_workers;
    json m_nativeTools = json::array();
    json m_pythonPaths;

    // 读取/搜索原文、缓存、字典、术语及备注；工具循环将异常回填为 error。
    json runReadTool(const fs::path& relInputPath, const std::string& name, const json& arguments);

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
    const std::function<std::string(std::string_view)>& m_onPerformApi;
    fs::path m_projectDir;
    fs::path m_transCacheDir;
    fs::path m_agentTermLedgerPath;
    fs::path m_agentFileNotesDir;
    std::string m_agentSystemPrompt;
    std::string m_agentUserPrompt;
    std::string m_targetLang;
    std::string m_apiStrategy;
    int m_maxRequestCount;
    int m_apiTimeOutMs;
    int m_agentMaxTurnsPerChunk;
    int m_agentCompactContextThresholdBytes;
    int m_agentSearchResultLimit;
    int m_agentContextLinesLimit;
    int m_inputBlockMaxLines;
    int m_problemMaxLines;
    int m_glossaryMaxLines;
    bool m_smartRetry;
    bool m_checkQuota;
    int m_enhanceJailbreak;
    std::shared_mutex& m_transCacheMutex;
    absl::flat_hash_map<fs::path, json>& m_savedTranslCacheMap;
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

    // 把 Agent 建议目标解析为相对文件和句子 id；file 为空时默认指向当前文件。
    std::optional<std::pair<fs::path, int>> parseAgentSuggestTarget(const json& suggestion, const fs::path& defaultRelPath) const;

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

    // 按引擎构造 JSONLINE 或 TSV 批次输入；小说格式不包含姓名。
    std::string buildInputBlock(std::span<Sentence*> pending) const;

    // 构造本批次开始前写入日志的可读摘要。
    std::string buildLogBlock(const fs::path& relInputPath, std::span<Sentence*> pending, const std::string& rollingContext);

    // 替换当前批次的原文、术语和备注占位符，追加到 worker 会话。
    // 压缩摘要仅在重建会话时单独注入，不放入每批提示词。
    std::string buildUserPrompt(const fs::path& relInputPath, std::span<Sentence*> pending, const std::string& rollingContext);

    // 校验 commit_translations 参数并写入译文，再更新术语账本、备注和建议。
    // 缺句或空译文通过工具 error 回填；附加信息写入失败只记录警告，不重试已提交译文。
    int applyCommit(const fs::path& relInputPath, std::span<Sentence*> pending,
        int threadId, const json& arguments, const std::string& modelName,
        const std::string& batchIndexLog, int turn, int requestCount,
        std::string& commitResultLog, int& recordedTermUpdateCount, int& recordedSuggestionCount);
};

NAMESPACE_END(gpp)
