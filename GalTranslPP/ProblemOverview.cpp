module;

#include "GPPMacros.hpp"

module ProblemOverview;

NAMESPACE_BEGIN(gpp)

bool ProblemOverviewTracker::usesJson(const toml& config)
{
    return tomlkit::findOr(config, "common", "problemOverviewFormat", "json") == "json";
}

fs::path ProblemOverviewTracker::overviewPath(const fs::path& projectDir, const toml& config)
{
    return projectDir / (usesJson(config) ? L"ProblemOverview.json" : L"ProblemOverview.toml");
}

std::string ProblemOverviewTracker::configKey(const toml& config)
{
    return usesJson(config)
        ? "GUIConfig.problemOverviewJsonLastWriteTime"
        : "GUIConfig.problemOverviewTomlLastWriteTime";
}

std::optional<tomlkit::OffsetDateTime> ProblemOverviewTracker::recordedLastWriteTime(const toml& config)
{
    const std::string_view key = usesJson(config)
        ? "problemOverviewJsonLastWriteTime"
        : "problemOverviewTomlLastWriteTime";
    const toml* pVal = config.find("GUIConfig", key);
    if (!pVal) {
        return std::nullopt;
    }
    if (pVal->isOffsetDateTime()) {
        return pVal->asOffsetDateTime();
    }
    // TODO: 未来大版本更新中移除旧版整数毫秒时间戳兼容
    if (pVal->isInteger()) {
        return tomlkit::OffsetDateTime::fromEpochMillisLocal(pVal->asInteger());
    }
    return std::nullopt;
}

bool ProblemOverviewTracker::hasUnimportedChanges(
    const fs::path& projectDir, const fs::path& cacheDirName, const toml& config)
{
    if (!fs::is_directory(projectDir / cacheDirName)) {
        return false;
    }
    const auto time = lastWriteTime(overviewPath(projectDir, config));
    const auto recorded = recordedLastWriteTime(config);
    return time && recorded && *time > *recorded;
}

bool ProblemOverviewTracker::isCurrentOverviewFile(
    const fs::path& path, const fs::path& projectDir, const toml& config)
{
    std::error_code error;
    return fs::equivalent(path, overviewPath(projectDir, config), error) && !error;
}

std::optional<tomlkit::OffsetDateTime> ProblemOverviewTracker::lastWriteTime(const fs::path& path)
{
    std::error_code error;
    if (!fs::is_regular_file(path, error)) {
        return std::nullopt;
    }
    const auto writeTime = fs::last_write_time(path, error);
    if (error) {
        return std::nullopt;
    }
#if defined(_LIBCPP_VERSION) && (!defined(__cpp_lib_chrono) || __cpp_lib_chrono < 201907L)
    // libc++ 的文件时钟提供 to_sys；不支持 clock_cast 时仍转换为同一个 Unix 时间轴。
    const auto systemTime = std::chrono::file_clock::to_sys(writeTime);
#else
    const auto systemTime = std::chrono::clock_cast<std::chrono::system_clock>(writeTime);
#endif
    const std::int64_t epochMs = std::chrono::floor<std::chrono::milliseconds>(systemTime.time_since_epoch()).count();
    return tomlkit::OffsetDateTime::fromEpochMillisLocal(epochMs);
}

bool ProblemOverviewTracker::recordLastWriteTime(
    toml& config, const fs::path& projectDir, const std::optional<fs::path>& importedPath)
{
    if (importedPath && !isCurrentOverviewFile(*importedPath, projectDir, config)) {
        return false;
    }
    const fs::path targetPath = importedPath ? *importedPath : overviewPath(projectDir, config);
    const auto time = lastWriteTime(targetPath);
    if (!time) {
        return false;
    }
    insertToml(config, configKey(config), *time);
    return true;
}

void ProblemOverviewTracker::recordLastWriteTime(
    const fs::path& projectDir, const std::optional<fs::path>& importedPath)
{
    const auto configPath = projectDir / L"Config.toml";
    auto config = parseToml(configPath);
    if (recordLastWriteTime(config, projectDir, importedPath)) {
        atomicOutputFile(configPath, tomlkit::format(config));
    }
}

ProblemOverviewImportResult importProblemOverview(const fs::path& projectDir, const fs::path& overviewPath)
{
    ProblemOverviewImportResult result;
    if (!fs::is_regular_file(overviewPath)) {
        throw std::invalid_argument(
            gppTr("ProblemOverview", "找不到问题概览文件 [%1]").arg(wide2Ascii(overviewPath)).toStdString());
    }
    json overview;
    if (isSameExtension(overviewPath, L".json")) {
        overview = parseJson(overviewPath);
    }
    else if (isSameExtension(overviewPath, L".toml")) {
        overview = toml2Json(parseToml(overviewPath).at("problemOverview"));
    }
    else {
        throw std::invalid_argument(gppTr("ProblemOverview", "未知的问题概览文件类型").toStdString());
    }

    std::map<std::string, std::vector<json>> files;
    for (const auto& item : overview) {
        files[item.at("filename").get<std::string>()].push_back(item);
    }
    for (const auto& [filename, items] : files) {
        const fs::path relativePath = ascii2Wide(filename);
        // 概览允许引用子目录，但不能越过本项目的缓存目录。
        if (relativePath.is_absolute() || std::ranges::find(relativePath, fs::path(L"..")) != relativePath.end()) {
            result.problems.push_back(
                gppTr("ProblemOverview", "[文件 %1] 不是有效的缓存相对路径，跳过导入").arg(filename).toStdString());
            continue;
        }
        const auto cachePath = projectDir / transCacheDirName / relativePath;
        if (!fs::exists(cachePath)) {
            result.problems.push_back(
                gppTr("ProblemOverview", "[文件 %1] 未在 cache 中找到，跳过导入").arg(filename).toStdString());
            continue;
        }

        json cache;
        std::unordered_map<int, json*> indices;
        try {
            cache = parseJson(cachePath);
            for (auto& item : cache) {
                indices.try_emplace(item.at("index").get<int>(), &item);
            }
        }
        catch (...) {
            result.problems.push_back(
                gppTr("ProblemOverview", "[文件 %1] 无法解析，跳过导入").arg(filename).toStdString());
            continue;
        }

        size_t imported = 0;
        for (const auto& item : items) {
            const int index = item.at("index").get<int>();
            const auto it = indices.find(index);
            if (it == indices.end()) {
                result.problems.push_back(
                    gppTr("ProblemOverview", "[文件 %1] 句子(index %2) 未在 cache 中找到，跳过导入")
                        .arg(filename)
                        .arg(index)
                        .toStdString());
                continue;
            }
            auto& cached = *it->second;
            if (item.at("original_text") != cached.at("original_text")) {
                result.problems.push_back(gppTr("ProblemOverview",
                    "[文件 %1] 句子(index %2) 与 cache 中原文不匹配，可能产生意外结果，\n概览原文: %3\n缓存原文: %4")
                        .arg(filename)
                        .arg(index)
                        .arg(item.at("original_text").get<std::string>())
                        .arg(cached.at("original_text").get<std::string>())
                        .toStdString());
            }
            cached = item;
            cached.erase("filename");
            ++imported;
        }
        if (imported == 0) {
            continue;
        }
        try {
            atomicOutputFile(cachePath, cache.dump(2));
            result.imported += imported;
        }
        catch (...) {
            result.problems.push_back(
                gppTr("ProblemOverview", "[文件 %1] 无法写入，跳过导入").arg(filename).toStdString());
        }
    }
    if (!result.problems.empty()) {
        std::string log;
        for (const auto& problem : result.problems) {
            log += problem + '\n';
        }
        log += gppTr("ProblemOverview", "成功导入 %1 个句子至 transl_cache").arg(result.imported).toStdString() + '\n';
        atomicOutputFile(projectDir / L"import_problems.log", log);
    }
    return result;
}

NAMESPACE_END(gpp)
