module;

#include "GPPMacros.hpp"

module AgentToolCommon;

import Tool;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

std::optional<int> parseAgentCommonJsonInt(const json& object) {
    if (object.is_number_integer()) {
        const std::int64_t value = object.get<std::int64_t>();
        if (value >= std::numeric_limits<int>::min() && value <= std::numeric_limits<int>::max()) {
            return (int)value;
        }
        return std::nullopt;
    }
    if (object.is_string()) {
        const std::string& value = object.get_ref<const std::string&>();
        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc{} && ptr == value.data() + value.size()) {
            return parsed;
        }
    }
    return std::nullopt;
}

int sanitizeAgentCommonToolLimit(int requested, int maxLimit) {
    if (requested <= 0) {
        return maxLimit;
    }
    return std::min(requested, maxLimit);
}

std::string safeRelativePath(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    const fs::path relPath = fs::relative(path, root, ec);
    if (ec) {
        return wide2Ascii(path.filename());
    }
    return wide2Ascii(relPath);
}

json runAgentCommonListFilesTool(
    const std::vector<fs::path>& relFiles,
    const std::function<std::optional<int>(const fs::path&)>& getFileLineCount,
    int searchResultLimit,
    const json& arguments
) {
    const std::wstring spec = str2Lower(ascii2Wide(arguments.value("spec", "")));
    const fs::path nativeSpec(spec);
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", searchResultLimit), searchResultLimit);
    json files = json::array();

    int matchCount = 0;
    for (const fs::path& relFile : relFiles) {
        if (!spec.empty()) {
            if (!str2Lower(relFile).contains(spec) && !matchFileSpec(relFile, nativeSpec.native()))
            {
                continue;
            }
        }
        ++matchCount;
        if (matchCount <= start || (int)files.size() >= limit) {
            continue;
        }

        json fileInfo = {
            {"file", wide2Ascii(relFile)}
        };
        if (getFileLineCount) {
            const std::optional<int> lineCount = getFileLineCount(relFile);
            if (lineCount.has_value()) {
                fileInfo["lines"] = lineCount.value();
            }
        }
        files.push_back(std::move(fileInfo));
    }
    return json{
        {"files", files},
        {"start", start},
        {"limit", limit},
        {"total", matchCount}
    };
}

json runAgentCommonGetProjectNoteTool(
    const fs::path& projectDir,
    const std::optional<fs::path>& projectNotePath,
    const json& arguments
) {
    if (!projectNotePath.has_value()) {
        return json{
            {"available", false},
            {"file", nullptr},
            {"content", ""}
        };
    }
    if (!fs::exists(projectNotePath.value())) {
        return json{
            {"available", false},
            {"file", safeRelativePath(projectNotePath.value(), projectDir)},
            {"content", ""}
        };
    }
    std::ifstream ifs(projectNotePath.value(), std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return json{
        {"file", safeRelativePath(projectNotePath.value(), projectDir)},
        {"content", content},
    };
}

NAMESPACE_END(gpp)
