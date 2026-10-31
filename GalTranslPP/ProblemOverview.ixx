module;

#include "GPPMacros.hpp"

export module ProblemOverview;

export import Tool;

export NAMESPACE_BEGIN(gpp)

struct ProblemOverviewImportResult {
    size_t imported = 0;
    std::vector<std::string> problems;
};

namespace ProblemOverviewTracker {

template<typename TC>
bool usesJson(const toml::basic_value<TC>& config)
{
    return toml::find_or(config, "common", "problemOverviewFormat", "json") == "json";
}

template<typename TC>
fs::path overviewPath(const fs::path& projectDir, const toml::basic_value<TC>& config)
{
    return projectDir / (usesJson(config) ? L"ProblemOverview.json" : L"ProblemOverview.toml");
}

template<typename TC>
std::string configKey(const toml::basic_value<TC>& config)
{
    return usesJson(config)
        ? "GUIConfig.problemOverviewJsonLastWriteTime"
        : "GUIConfig.problemOverviewTomlLastWriteTime";
}

// 与 GUI 原有记录一致，使用 Unix 时间戳，单位为毫秒。
std::optional<std::int64_t> lastWriteTime(const fs::path& path);

template<typename TC>
std::optional<std::int64_t> recordedLastWriteTime(const toml::basic_value<TC>& config)
{
    constexpr std::int64_t unrecorded = std::numeric_limits<std::int64_t>::min();
    const std::int64_t time = usesJson(config)
        ? toml::find_or(config, "GUIConfig", "problemOverviewJsonLastWriteTime", unrecorded)
        : toml::find_or(config, "GUIConfig", "problemOverviewTomlLastWriteTime", unrecorded);
    if (time == unrecorded) {
        return std::nullopt;
    }
    return time;
}

template<typename TC>
bool hasUnimportedChanges(const fs::path& projectDir, const fs::path& cacheDirName,
    const toml::basic_value<TC>& config)
{
    if (!fs::is_directory(projectDir / cacheDirName)) {
        return false;
    }
    const auto time = lastWriteTime(overviewPath(projectDir, config));
    const auto recorded = recordedLastWriteTime(config);
    return time && recorded && *time > *recorded;
}

template<typename TC>
bool isCurrentOverviewFile(const fs::path& path, const fs::path& projectDir,
    const toml::basic_value<TC>& config)
{
    std::error_code error;
    return fs::equivalent(path, overviewPath(projectDir, config), error) && !error;
}

// CLI 导入或完成翻译后保存记录；导入其它文件时不改变当前清单的时间戳。
void recordLastWriteTime(const fs::path& projectDir, const std::optional<fs::path>& importedPath = std::nullopt);

}

// 按文件名和 index 回填整条缓存记录，GUI 与 CLI 使用相同的导入语义。
ProblemOverviewImportResult importProblemOverview(const std::filesystem::path& projectDir,
    const std::filesystem::path& overviewPath);

NAMESPACE_END(gpp)
