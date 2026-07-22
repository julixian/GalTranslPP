#ifndef PROBLEMOVERVIEWTRACKER_H
#define PROBLEMOVERVIEWTRACKER_H

#include <QDateTime>
#include <QFileInfo>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>

#include <toml.hpp>

namespace ProblemOverviewTracker
{
    namespace fs = std::filesystem;

    inline bool usesJson(const toml::ordered_value& projectConfig)
    {
        return toml::find_or(projectConfig, "common", "problemOverviewFormat", "json") == "json";
    }

    inline fs::path overviewPath(const fs::path& projectDir, const toml::ordered_value& projectConfig)
    {
        return projectDir / (usesJson(projectConfig) ? L"ProblemOverview.json" : L"ProblemOverview.toml");
    }

    inline std::string configKey(const toml::ordered_value& projectConfig)
    {
        return usesJson(projectConfig)
            ? "GUIConfig.problemOverviewJsonLastWriteTime"
            : "GUIConfig.problemOverviewTomlLastWriteTime";
    }

    inline std::optional<std::int64_t> lastWriteTime(const fs::path& path)
    {
        const QFileInfo fileInfo(QString::fromStdWString(path.wstring()));
        if (!fileInfo.isFile()) {
            return std::nullopt;
        }
        return fileInfo.lastModified().toMSecsSinceEpoch();
    }

    inline std::optional<std::int64_t> recordedLastWriteTime(const toml::ordered_value& projectConfig)
    {
        constexpr std::int64_t unrecorded = std::numeric_limits<std::int64_t>::min();
        const std::int64_t recordedTime = usesJson(projectConfig)
            ? toml::find_or(projectConfig, "GUIConfig", "problemOverviewJsonLastWriteTime", unrecorded)
            : toml::find_or(projectConfig, "GUIConfig", "problemOverviewTomlLastWriteTime", unrecorded);
        if (recordedTime == unrecorded) {
            return std::nullopt;
        }
        return recordedTime;
    }

    inline bool hasUnimportedChanges(const fs::path& projectDir, const toml::ordered_value& projectConfig)
    {
        const QFileInfo cacheDirInfo(QString::fromStdWString((projectDir / L"trans_cache").wstring()));
        if (!cacheDirInfo.isDir()) {
            return false;
        }
        const auto writeTime = lastWriteTime(overviewPath(projectDir, projectConfig));
        const auto recordedTime = recordedLastWriteTime(projectConfig);
        return writeTime && recordedTime && *writeTime > *recordedTime;
    }

    inline bool isCurrentOverviewFile(const fs::path& path, const fs::path& projectDir,
        const toml::ordered_value& projectConfig)
    {
        std::error_code error;
        return fs::equivalent(path, overviewPath(projectDir, projectConfig), error) && !error;
    }
}

#endif
