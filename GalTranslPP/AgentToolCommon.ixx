module;

#include "GPPMacros.hpp"

export module AgentToolCommon;

export import GPPDefines;
export import AgentCommonSourceView;

namespace fs = std::filesystem;

export
{
    struct AgentCommonToolCallRequest {
        std::string id;
        std::string name;
        json arguments = json::object();
    };

    // 解析整数 JSON 值，或解析字符串形式的十进制整数。
    std::optional<int> parseAgentCommonJsonInt(const json& object);

    // 估算请求 messages 的字节数，供上下文压缩阈值判断使用。
    size_t approximateAgentCommonMessagesBytes(const json& messages);

    // 把协议中的工具调用对象解析为统一的 AgentCommonToolCallRequest。
    std::vector<AgentCommonToolCallRequest> parseAgentCommonToolCallRequests(const json& payload);

    // 格式化工具调用的名称和参数明细。
    std::string formatAgentCommonToolCallDetails(const std::vector<AgentCommonToolCallRequest>& calls);

    // 格式化工具调用名称列表。
    std::string formatAgentCommonToolCallNames(const std::vector<AgentCommonToolCallRequest>& calls);

    // 尽可能把路径转换为稳定的项目相对字符串。
    std::string safeRelativePath(const fs::path& path, const fs::path& root);

    // 从模型文本中提取协议 JSON 外壳，允许代码块、轻量修复和外层说明文本。
    std::optional<json> tryParseAgentCommonJsonEnvelope(const std::string& text);

    // 把工具返回数量限制夹到有效范围内，且不超过配置上限。
    int sanitizeAgentCommonToolLimit(int requested, int maxLimit);

    // 把上下文行数限制夹到配置上限内；配置为 0 时允许返回 0。
    int sanitizeAgentCommonContextLines(int requested, int maxLimit);

    // 从工具参数中收集 query 或 queries 字段。
    std::vector<std::string> collectAgentCommonToolQueries(const json& arguments);

    // 执行翻译和审校 Agent 共用的 list_files 工具。
    json runAgentCommonListFilesTool(
        const std::vector<fs::path>& relFiles,
        const std::function<std::optional<int>(const fs::path&)>& getFileLineCount,
        int searchResultLimit,
        const json& arguments
    );

    // 执行翻译和审校 Agent 共用的 get_project_note 工具。
    json runAgentCommonGetProjectNoteTool(
        const fs::path& projectDir,
        const std::optional<fs::path>& projectNotePath,
        const json& arguments
    );

    // 执行翻译和审校 Agent 共用的 search_text 搜索原语。
    json runAgentCommonSourceSearchTextTool(
        const fs::path& currentFile,
        const std::vector<fs::path>& relFiles,
        const std::function<const AgentCommonSourceFileView*(const fs::path&)>& findSourceFile,
        int searchResultLimit,
        int contextLinesLimit,
        bool requireQuery,
        const std::string& invalidScopeErrorMessage,
        const json& arguments
    );
}
