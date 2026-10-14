module;

#include "GPPMacros.hpp"

export module AgentToolCommon;

export import AgentCommonSourceView;

export NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

// 解析整数 JSON 值，或解析字符串形式的十进制整数。
std::optional<int> parseAgentCommonJsonInt(const json& object);

// 尽可能把路径转换为稳定的项目相对字符串。
std::string safeRelativePath(const fs::path& path, const fs::path& root);

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
