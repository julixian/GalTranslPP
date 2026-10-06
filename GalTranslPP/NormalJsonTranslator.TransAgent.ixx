module;

#include "GPPMacros.hpp"

export module NormalJsonTranslator:TransAgent;

export import AgentToolCommon;
export import ApiPool;
export import Dictionary;
export import ITranslator;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

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
// 普通路径：performApiRequest -> parseAndApplyTurnResponse -> parseProtocolResponse -> executeToolCalls / applyCommit。
// 高级路径：translateAdvancedBatch -> performAdvancedAgentApiRequest -> 逐个解析原生工具参数 -> runReadTool / applyCommit。
// 两条路径共用查询工具和提交校验；API 失败在请求循环重试，单个查询工具失败回填给模型。
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
        bool enhanceJailbreak,
        std::shared_mutex& transCacheMutex,
        absl::flat_hash_map<fs::path, json>& savedTranslCacheMap,
        const std::vector<fs::path>& knownRelFiles,
        const std::vector<fs::path>& gptDictionaryPaths,
        const std::optional<fs::path>& agentProjectNotePath,
        const std::function<void(Sentence*)>& preProcessFunc
    );

    // NormalJsonTranslator::processFile 的批次入口；高级模式转给 translateAdvancedBatch，否则运行文本动作协议。
    // 普通路径分别限制轮次和每轮请求次数：ApiError 交给 handleApiError，动作解析/提交错误记录后重试当前轮。
    // 完成返回 true；停止或耗尽限制返回 false，耗尽时给剩余句子写失败标记。
    // 没有可用 API 时抛 runtime_error，经 processFile 传给上层文件任务错误处理。
    bool translateBatch(const fs::path& relInputPath, std::span<Sentence*> batch, std::string& rollingContext,
        int& recursionIndex, int& recursionCount, int threadId, int batchIndex);

    // 把提交阶段记录的跨文件 Agent 建议写入翻译缓存的 problems 字段。
    void applyAgentSuggestions();

    // normalJsonBeforeRun 在构造 Agent 后调用，按 worker 数建立会话槽并生成原生工具声明。
    // 此处不发请求、不解析响应；初选不到 API 的会话槽保持为空，实际翻译时再选择。
    void configureAdvanced(bool enabled, int workerCount);

private:
    struct TransAdvancedAgentWorker {
        std::optional<AdvancedAgentApiSession> session;
        std::string rollingContext;
    };

    bool m_advancedEnabled = false;
    std::vector<TransAdvancedAgentWorker> m_workers;
    json m_nativeTools = json::array();

    // translateBatch 的高级分支；按从 1 开始的 threadId 复用 worker 会话，选择 API、处理压缩并驱动原生工具轮次。
    // API 失败经 handleApiError 重试；工具参数 JSON、查询和提交的 std::exception 按调用 id 回填 error，进入下一轮。
    // 手动摘要无效或压缩请求耗尽时清空旧历史，用已有滚动记忆重建；有效摘要则先替换滚动记忆。
    // 成功提交返回 true；停止、重试/轮次/连续无工具调用次数耗尽返回 false，耗尽时标记剩余句子失败。
    // 没有可用 API 时抛 runtime_error，沿 translateBatch、processFile 传给上层文件任务错误处理。
    bool translateAdvancedBatch(const fs::path& relInputPath, std::span<Sentence*> batch,
        std::string& rollingContext, int threadId, int batchIndex);
    // 普通 executeToolCalls 和高级工具循环共用的查询分发，读取/搜索原文、缓存、字典、术语及备注。
    // 检查工具域和原文/缓存目标文件，限制分页数量与上下文行数，按 fields 投影返回数据；不提交译文。
    // 未知工具/文件、参数类型或读取失败可抛异常，不在这里捕获；两个调用方各自将 std::exception 回填为工具 error。
    json runReadTool(const fs::path& relInputPath, const std::string& name, const json& arguments);

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
    bool m_enhanceJailbreak;
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

    // 普通 parseAndApplyTurnResponse 解析模型正文；高级 commit_translations 先补 action=commit，再复用本函数。
    // 提取 JSON 对象，检查 action 存在且为 tool_calls/compact_context/commit，tool_calls 必须有调用；归一化提交字段。
    // 不检查每句译文是否齐全，由 applyCommit 负责；非法协议抛 runtime_error，字段类型错误也可抛 JSON 异常。
    // 普通路径由 parseAndApplyTurnResponse 转成错误字符串重试；高级路径由工具循环捕获并回填 error。
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

    // 普通 parseAndApplyTurnResponse 的 tool_calls 分支调用，分发查询并汇总回填 JSON、摘要和调试明细。
    // 每个 runReadTool 的 std::exception 单独转成 result.error，其它工具继续执行，该轮仍可正常进入下一轮。
    TransAgentToolCallResult executeToolCalls(
        const fs::path& relInputPath,
        const std::vector<AgentCommonToolCallRequest>& calls,
        bool collectDetail
    );

    // 仅普通 translateBatch 在 API 成功后调用：parseProtocolResponse -> 查询工具/重建压缩消息/applyCommit。
    // 成功返回 ContinueTurn 或 CompleteBatch；捕获整个动作处理中的 std::exception，返回 unexpected<string>。
    // 外层据此记录业务错误、增加请求次数并重试当前轮；单个查询工具的 error 回填不算整轮解析失败。
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

    // 普通批次初始化/压缩时构造 system 与 user 消息，并替换术语、备注、滚动上下文等占位符。
    // 高级路径只取这里的 user 批次数据追加到持久会话；system 由 AdvancedAgentApiSession 单独保存。
    // 调用方先收集待翻译句子，保证 pending 非空。
    json buildBaseMessages(const fs::path& relInputPath, std::span<Sentence*> pending, const std::string& rollingContext);

    // 普通 commit 动作与高级 commit_translations 共用；先按 id 收集译文，检查每个 pending 句子均有非空 dst，再写入。
    // 缺句、空译文或字段类型错误抛异常；普通路径转成整轮业务错误重试，高级工具循环回填 error 让模型修正提交。
    // 译文写入后再更新滚动记忆、术语账本、备注和建议；账本/建议及备注处理的 std::exception 在内部记录警告，
    // 保留已经提交的译文，不因此重试整批；返回实际提交句数，并通过输出参数提供日志及术语/建议计数。
    int applyCommit(const fs::path& relInputPath, std::span<Sentence*> pending, std::string& rollingContext,
        int threadId, const TransAgentProtocolResponse& protocol, const std::string& modelName,
        const std::string& batchIndexLog, int turn, int requestCount,
        std::string& commitResultLog, int& recordedTermUpdateCount, int& recordedSuggestionCount);
};

NAMESPACE_END(gpp)
