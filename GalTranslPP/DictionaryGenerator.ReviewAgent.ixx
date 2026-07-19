module;

#include "GPPMacros.hpp"

export module DictionaryGenerator:ReviewAgent;

export import AgentToolCommon;
export import ApiPool;
export import AgentCommonSourceView;
export import DictionaryReviewIndex;
export import GPPDefines;
export import ITranslator;

namespace fs = std::filesystem;

export
{
    struct DictionaryReviewAgentCommitResult {
        std::string sourceTerm;
        std::string finalTarget;
        std::string finalNote;
        std::string status;
        std::string mergeInto;
        json termUpdates = json::array();
    };

    struct DictionaryReviewAgentProtocolResponse {
        std::string action;
        std::vector<AgentCommonToolCallRequest> calls;
        DictionaryReviewAgentCommitResult result;
    };

    // DictionaryGenerator 的字典审校 Agent 工作流对象。
    //
    // 调用流程：
    // 1. DictionaryGenerator::generate() 传入粗提取结果、计数和源文件视图。
    // 2. review() 在 Agent 内部聚合 DictionaryReviewTermGroup，并建立账本和索引。
    // 3. runReviewWorkers() 启动 worker，每个 worker 调用 reviewTermGroup() 处理一个术语。
    // 4. reviewTermGroup() 驱动“模型请求 -> 工具调用/跳过/提交”的循环。
    // 5. applyCommitResultLocked() 校验提交结果并写入账本，最后 buildFinalDictionary() 输出账本里保留的审校结果。
    //
    // 示例：
    //   auto reviewAgent = std::make_unique<DictionaryGeneratorReviewAgent>(
    //       m_controller, m_logger, m_apiPool, m_onPerformApi, m_projectDir, ...);
    //   DictList reviewed = reviewAgent->review(m_finalDict, m_finalCounter, m_segments, selectedIndices, ...);
    class DictionaryGeneratorReviewAgent {
    public:
        // 保存本轮审校依赖；粗候选和源文件视图由 review() 接收并组织。
        DictionaryGeneratorReviewAgent(
            const std::shared_ptr<IController>& controller,
            const std::shared_ptr<spdlog::logger>& logger,
            const std::unique_ptr<ApiPool>& apiPool,
            const std::function<std::string(std::string)>& onPerformApi,
            const fs::path& projectDir,
            const std::vector<fs::path>& relJsonPaths,
            const std::optional<fs::path>& agentProjectNotePath,
            const std::string& genDictReviewSystemPrompt,
            const std::string& genDictReviewUserPrompt,
            const std::string& apiStrategy,
            const std::string& targetLang,
            int inputBlockMaxLines,
            int maxRequestCount,
            int threadsNum,
            int agentMaxTurnsPerChunk,
            int agentSearchResultLimit,
            int agentContextLinesLimit,
            int apiTimeOutMs,
            bool checkQuota
        );

        // 审校粗候选术语，并返回最终保留的字典词条。
        DictList review(
            const DictList& coarseCandidates,
            const absl::flat_hash_map<std::string, int>& finalCounter,
            const std::vector<std::string>& segments,
            const std::vector<int>& selectedIndices,
            const absl::flat_hash_set<std::string>& nameSet,
            const absl::flat_hash_map<std::string, int>& wordCounter,
            const std::vector<AgentCommonSourceFileView>& sourceFiles
        );

    private:
        struct DictionaryReviewAgentTurnResult {
            enum class Action {
                ContinueTurn,
                CompleteTerm
            };
            Action action = Action::ContinueTurn;
            std::string summary;
        };

        struct DictionaryReviewToolCallResult {
            json results = json::array();
            std::string summary;
            std::string detail;
        };

        std::shared_ptr<IController> m_controller;
        std::shared_ptr<spdlog::logger> m_logger;
        const std::unique_ptr<ApiPool>& m_apiPool;
        const std::function<std::string(std::string)>& m_onPerformApi;
        fs::path m_projectDir;
        std::vector<fs::path> m_relJsonPaths;
        std::optional<fs::path> m_agentProjectNotePath;
        std::string m_genDictReviewSystemPrompt;
        std::string m_genDictReviewUserPrompt;
        std::string m_apiStrategy;
        std::string m_targetLang;
        int m_inputBlockMaxLines;
        int m_maxRequestCount;
        int m_threadsNum;
        int m_agentMaxTurnsPerChunk;
        int m_agentSearchResultLimit;
        int m_agentContextLinesLimit;
        int m_apiTimeOutMs;
        bool m_checkQuota;
        std::vector<DictionaryReviewTermGroup> m_groups;
        const std::vector<AgentCommonSourceFileView>* m_sourceFiles = nullptr;
        absl::flat_hash_map<fs::path, const AgentCommonSourceFileView*> m_sourceFileLookup;
        absl::flat_hash_map<std::string, const DictionaryReviewTermGroup*> m_groupLookup;
        absl::flat_hash_map<std::string, json> m_ledgerMap;
        absl::flat_hash_set<std::string> m_knownSourceTerms;
        absl::flat_hash_set<std::string> m_ledgerTermOrderSeen;
        std::vector<std::string> m_ledgerTermOrder;
        std::mutex m_ledgerMutex;

        // 在持有 m_ledgerMutex 时记录稳定输出顺序，重复术语会被过滤。
        void rememberLedgerTermOrderLocked(const std::string& sourceTerm);

        // 在持有 m_ledgerMutex 时把模型给出的 term_update 合并进账本。
        void applyTermUpdateLocked(const json& update);

        // 在持有 m_ledgerMutex 时校验并应用一个提交结果。
        void applyCommitResultLocked(const DictionaryReviewTermGroup& group, const DictionaryReviewAgentCommitResult& decision);

        // 解析并校验审校 Agent 文本协议，得到动作、工具调用和结果字段。
        DictionaryReviewAgentProtocolResponse parseProtocolResponse(const std::string& content) const;

        // 通过扫描说话人和文本内容，推断术语最相关的源文件。
        fs::path guessCurrentFileForTerm(const std::string& sourceTerm) const;

        // 返回审校术语组里频率最高的候选译名，供工具结果展示。
        std::string candidateTargetForGroup(const DictionaryReviewTermGroup& group) const;

        // 返回审校术语组里频率最高的候选备注，供工具结果展示。
        std::string candidateNoteForGroup(const DictionaryReviewTermGroup& group) const;

        // 为一个术语组构造审校 Agent 模型 messages。
        json buildBaseMessages(const DictionaryReviewTermGroup& group, const fs::path& currentFile, const json& ledgerExcerpt) const;

        // 把待审校或已审校术语组转换为 search_dictionary 结果行。
        json candidateToDictionarySearchJson(const DictionaryReviewTermGroup& group, const json* reviewedEntry) const;

        // 把只存在于账本中的条目转换为 search_dictionary 结果行。
        json ledgerOnlyToDictionarySearchJson(const std::string& sourceTerm, const json& entry) const;

        // 构造审校候选项的小写搜索文本。
        std::string candidateSearchHaystack(const DictionaryReviewTermGroup& group, const json* reviewedEntry) const;

        // 构造账本独有审校项的小写搜索文本。
        std::string ledgerOnlySearchHaystack(const std::string& sourceTerm, const json& entry) const;

        // 格式化最终写入的审校账本项及相关更新。
        std::string formatAppliedEntry(const std::string& sourceTerm, const json& entry, const json& termUpdates) const;

        // 查找审校源文件视图，供 search_text 和 list_files 共享。
        const AgentCommonSourceFileView* findSourceFileView(const fs::path& relPath) const;

        // 返回审校源文件行数，供 list_files 展示。
        std::optional<int> getSourceFileLineCount(const fs::path& relPath) const;

        // 执行审校 search_text，在当前/指定/全部文件中查找术语上下文。
        json runSearchTextTool(const fs::path& currentFile, const json& arguments) const;

        // 执行审校 search_dictionary，在粗候选和已审校账本快照中搜索术语。
        json runSearchDictionaryTool(
            const std::vector<std::string>& ledgerTermOrder,
            const absl::flat_hash_map<std::string, json>& ledgerMap,
            const json& arguments
        ) const;

        // 分发当前术语轮次请求的工具调用，并合并回填 JSON、摘要和调试明细。
        DictionaryReviewToolCallResult executeToolCalls(
            const fs::path& currentFile,
            const std::vector<std::string>& ledgerTermOrder,
            const absl::flat_hash_map<std::string, json>& ledgerMap,
            const std::vector<AgentCommonToolCallRequest>& calls,
            bool collectDetail
        ) const;

        // 解析一轮审校 Agent 响应，并执行工具调用、跳过或提交。
        std::expected<DictionaryReviewAgentTurnResult, std::string> parseAndApplyTurnResponse(
            const fs::path& currentFile,
            const DictionaryReviewTermGroup& group,
            json& messages,
            const std::string& content,
            const std::string& reviewIndexLog,
            int turn,
            int requestCount,
            int threadId
        );

        // 审校一个粗候选术语组，直到提交、跳过、请求耗尽或达到轮数上限。
        void reviewTermGroup(int groupIndex, const DictionaryReviewTermGroup& group, int threadId);

        // 启动术语审校 worker，并等待所有术语审校结束。
        void runReviewWorkers();

        // 按粗候选顺序和模型新增顺序生成最终保留词条；未进入账本的术语不补默认候选。
        DictList buildFinalDictionary() const;
    };
}
