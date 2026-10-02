module;

#include "GPPMacros.hpp"

#ifdef _WIN32
#include <Shlwapi.h>
#endif

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
        const std::string value = object.get<std::string>();
        int parsed = 0;
        const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), parsed);
        if (ec == std::errc{} && ptr == value.data() + value.size()) {
            return parsed;
        }
    }
    return std::nullopt;
}

size_t approximateAgentCommonMessagesBytes(const json& messages) {
    return std::ranges::fold_left(messages, 0uz, [](size_t acc, const auto& item)
        {
            return acc + item.dump().size();
        });
}

std::optional<json> tryParseAgentCommonJsonEnvelope(const std::string& text) {
    std::string newText = text;
    if (newText.empty()) {
        return std::nullopt;
    }

    if (size_t pos = newText.find("</think>"); pos != std::string::npos) {
        newText = newText.substr(pos + 8);
    }
    else if (pos = newText.find("<end_think>"); pos != std::string::npos) {
        newText = newText.substr(pos + 11);
    }

    const size_t jsonStart = newText.find('{');
    const size_t jsonEnd = newText.rfind('}');
    if (jsonStart == std::string::npos || jsonEnd == std::string::npos || jsonEnd <= jsonStart) {
        return std::nullopt;
    }

    const std::string jsonSlice = newText.substr(jsonStart, jsonEnd - jsonStart + 1);
    try {
        return json::parse(jsonSlice);
    }
    catch (...) { }

    try {
        return json::parse(lightRepairJsonText(jsonSlice));
    }
    catch (...) { }

    return std::nullopt;
}

std::vector<AgentCommonToolCallRequest> parseAgentCommonToolCallRequests(const json& payload) {
    std::vector<AgentCommonToolCallRequest> calls;
    if (const auto it = payload.find("calls"); it != payload.end() && it->is_array()) {
        for (const auto& call : *it) {
            if (!call.is_object()) {
                continue;
            }
            AgentCommonToolCallRequest parsed;
            parsed.id = call.value("id", std::format("call_{}", calls.size()));
            parsed.name = call.value("name", "");
            if (const auto argIt = call.find("arguments"); argIt != call.end()) {
                parsed.arguments = *argIt;
            }
            calls.push_back(std::move(parsed));
        }
    }
    return calls;
}

std::string formatAgentCommonToolCallDetails(const std::vector<AgentCommonToolCallRequest>& calls) {
    std::string result;
    for (const auto& [index, call] : calls | std::views::enumerate) {
        result += std::format(
            "[{}] {}({})\n",
            index + 1,
            call.name.empty() ? "<unknown>" : call.name,
            call.arguments.dump(2)
        );
    }
    return result;
}

std::string formatAgentCommonToolCallNames(const std::vector<AgentCommonToolCallRequest>& calls) {
    if (calls.empty()) {
        return "None";
    }
    std::string result;
    for (const auto& [index, call] : calls | std::views::enumerate) {
        if (index > 0) {
            result += ", ";
        }
        result += call.name.empty() ? "<unknown>" : call.name;
    }
    return result;
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
    const int start = std::max(0, arguments.value("start", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", searchResultLimit), searchResultLimit);
    json files = json::array();

    int matchCount = 0;
    for (const fs::path& relFile : relFiles) {
        if (!spec.empty()) {
            if (!str2Lower(relFile.wstring()).contains(spec) &&
#ifdef _WIN32
                !PathMatchSpecW(relFile.c_str(), spec.c_str())
#endif
                )
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
