module;

#include "GPPMacros.hpp"

export module AgentToolCommon;

export import AgentCommonSourceView;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

struct AgentCommonToolCallRequest {
    std::string id;
    std::string name;
    json arguments = json::object();
};

// 解析整数 JSON 值，或解析字符串形式的十进制整数。
std::optional<int> parseAgentCommonJsonInt(const json& object);

// 估算请求 messages 的字节数，供上下文压缩阈值判断使用。
size_t approximateAgentCommonMessagesBytes(const json& messages);

// parseProtocolResponse 将普通 Agent 文本协议的 calls 转为统一对象；忽略非对象项，保留原始 arguments。
// 不执行工具，也不检查参数是否合法；id/name 类型错误可抛 JSON 异常，普通路径由 parseAndApplyTurnResponse 捕获，
// 高级路径复用提交协议解析时则由逐工具 catch 捕获。
std::vector<AgentCommonToolCallRequest> parseAgentCommonToolCallRequests(const json& payload);

// 格式化工具调用的名称和参数明细。
std::string formatAgentCommonToolCallDetails(const std::vector<AgentCommonToolCallRequest>& calls);

// 格式化工具调用名称列表。
std::string formatAgentCommonToolCallNames(const std::vector<AgentCommonToolCallRequest>& calls);

// 尽可能把路径转换为稳定的项目相对字符串。
std::string safeRelativePath(const fs::path& path, const fs::path& root);

// parseProtocolResponse 使用的文本 JSON 提取：去掉思考前缀，截取第一个 { 到最后一个 }，先解析再尝试轻量修复。
// 两次解析各自捕获所有异常，失败返回 nullopt；不校验 action 或业务字段，由 parseProtocolResponse 继续检查。
std::optional<json> tryParseAgentCommonJsonEnvelope(const std::string& text);

// 把工具返回数量限制夹到有效范围内，且不超过配置上限。
int sanitizeAgentCommonToolLimit(int requested, int maxLimit);

// 执行 TransAgent 的 list_files 工具。
json runAgentCommonListFilesTool(
    const std::vector<fs::path>& relFiles,
    const std::function<std::optional<int>(const fs::path&)>& getFileLineCount,
    int searchResultLimit,
    const json& arguments
);

// 读取 TransAgent 配置的项目备注。
json runAgentCommonGetProjectNoteTool(
    const fs::path& projectDir,
    const std::optional<fs::path>& projectNotePath,
    const json& arguments
);

NAMESPACE_END(gpp)
