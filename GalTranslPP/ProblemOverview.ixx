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

bool usesJson(const toml& config);
fs::path overviewPath(const fs::path& projectDir, const toml& config);
std::string configKey(const toml& config);

// 记录带本地时区偏移的毫秒级日期时间，同时兼容读取旧版整数毫秒时间戳。
std::optional<tomlkit::OffsetDateTime> lastWriteTime(const fs::path& path);
std::optional<tomlkit::OffsetDateTime> recordedLastWriteTime(const toml& config);
bool hasUnimportedChanges(const fs::path& projectDir, const fs::path& cacheDirName, const toml& config);
bool isCurrentOverviewFile(const fs::path& path, const fs::path& projectDir, const toml& config);

// 更新内存配置中的当前问题清单修改时间记录；导入其它文件或文件不存在时返回 false。
bool recordLastWriteTime(toml& config, const fs::path& projectDir, const std::optional<fs::path>& importedPath = std::nullopt);
// CLI 导入或完成翻译后读取并保存记录到 Config.toml；导入其它文件时不改变当前清单的时间戳。
void recordLastWriteTime(const fs::path& projectDir, const std::optional<fs::path>& importedPath = std::nullopt);

}

// 按文件名和 index 回填整条缓存记录，GUI 与 CLI 使用相同的导入语义。
ProblemOverviewImportResult importProblemOverview(const std::filesystem::path& projectDir,
    const std::filesystem::path& overviewPath);

NAMESPACE_END(gpp)
