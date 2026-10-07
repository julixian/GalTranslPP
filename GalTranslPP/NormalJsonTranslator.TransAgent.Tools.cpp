module;

#include "GPPMacros.hpp"

module NormalJsonTranslator;

import NormalJsonTranslatorHelperTool;
import Tool;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

json projectAgentRow(const json& row, const json& fields)
{
    // file/id 始终返回；fields 只控制数据列，不影响搜索使用的 match_fields。
    json projected = {{"file", row.value("file", "")}, {"id", row.at("id")}};
    if (fields.empty()) {
        return row;
    }
    for (const auto& field : fields) {
        const std::string& key = field.get_ref<const std::string&>();
        if (row.contains(key)) {
            projected[key] = row.at(key);
        }
    }
    return projected;
}

// 两种 Agent 路径共用读、搜索和备注工具，协议层只负责回填消息的形式。
json NormalJsonTranslatorTransAgent::runReadTool(const fs::path& relInputPath, const std::string& name, const json& arguments)
{
    if (name == "list_files") {
        json args = arguments;
        args["start"] = arguments.value("offset", 0);
        args["limit"] = sanitizeAgentCommonToolLimit(arguments.value("limit", 0), m_agentSearchResultLimit);
        json result = runAgentCommonListFilesTool(m_knownRelFiles,
            [this](const fs::path& path) { return getSourceFileLineCount(path); }, m_agentSearchResultLimit, args);
        result["offset"] = std::move(result.at("start"));
        result.erase("start");
        return result;
    }
    if (name == "read_file_note") {
        const std::string file = arguments.value("file", "");
        const fs::path targetRelPath = file.empty() ? relInputPath : fs::path(ascii2Wide(file));
        return {{"file", wide2Ascii(targetRelPath)}, {"note", loadFileNote(targetRelPath)}};
    }
    if (name == "read_project_note") {
        return runAgentCommonGetProjectNoteTool(m_projectDir, m_agentProjectNotePath, arguments);
    }
    const bool search = name.starts_with("search_");
    const std::string domain = name.substr(search ? 7 : 5);
    json rows = json::array();
    if (domain == "source" || domain == "cache") {
        const std::string file = arguments.value("file", "");
        const fs::path requested = file.empty() ? relInputPath : fs::path(ascii2Wide(file));
        if (file != "*" && !findSourceFileView(requested)) {
            throw std::runtime_error("Unknown source file: " + wide2Ascii(requested));
        }
        for (const auto& path : m_knownRelFiles) {
            if (file != "*" && path != requested) {
                continue;
            }
            if (domain == "source") {
                if (const auto* view = findSourceFileView(path)) {
                    for (const auto& line : view->lines) {
                        rows.push_back({ {"file", wide2Ascii(path)}, {"id", line.id}, {"name", line.speaker}, {"src", line.sourceText} });
                    }
                }
            }
            else {
                auto cached = loadCacheDstMap(path);
                std::vector<int> ids = cached | std::views::keys | std::ranges::to<std::vector>();
                std::ranges::sort(ids);
                for (int id : ids) {
                    const auto& entry = cached.at(id);
                    rows.push_back({{"file", wide2Ascii(path)}, {"id", id}, {"name", getNameString(entry)},
                        {"src", entry.value("pre_processed_text", entry.value("original_text", ""))},
                        {"dst", entry.value("translated_raw_text", "")}});
                }
            }
        }
    }
    else if (domain == "dictionary") {
        int id = 0;
        for (const auto& entry : loadDictionaryEntries()) {
            rows.push_back({ {"file", ""}, {"id", id++},
            {"src", entry.sourceTerm}, {"dst", entry.targetTerm}, {"note", entry.note} });
        }
    }
    else if (domain == "terms") {
        int id = 0;
        const json ledger = loadTermLedger();
        for (const auto& entry : ledger.items()) {
            json row = entry.value();
            row["file"] = "";
            row["id"] = id++;
            row["src"] = entry.key();
            row["dst"] = row.value("target_term", "");
            row.erase("target_term");
            rows.push_back(std::move(row));
        }
    }
    else {
        throw std::runtime_error("Unknown tool: " + name);
    }

    const int offset = std::max(0, arguments.value("offset", 0));
    const int limit = sanitizeAgentCommonToolLimit(arguments.value("limit", 0), m_agentSearchResultLimit);
    const int before = std::clamp(arguments.value("context_before", 0), 0, m_agentContextLinesLimit);
    const int after = std::clamp(arguments.value("context_after", 0), 0, m_agentContextLinesLimit);
    const json ids = arguments.value("ids", json::array());
    const json fields = arguments.value("fields", json::array());
    const json matchFields = arguments.value("match_fields", json::array());
    const std::string query = str2Lower(arguments.value("query", ""));
    json results = json::array();
    int total = 0;
    for (int i = 0; i < (int)rows.size(); ++i) {
        const auto& row = rows[i];
        if (!ids.empty() && std::ranges::none_of(ids, [&](const json& id) { return id == row.at("id"); })) {
            continue;
        }
        bool matched = !search || query.empty();
        if (!matched) {
            for (const auto& entry : row.items()) {
                if (entry.key() == "file" || entry.key() == "id") {
                    continue;
                }
                if (!matchFields.empty() && std::ranges::none_of(matchFields, [&](const json& field) { return field == entry.key(); })) {
                    continue;
                }
                const std::string textLower = entry.value().is_string() ? str2Lower(entry.value().get_ref<const std::string&>()) : str2Lower(entry.value().dump());
                if (textLower.contains(query)) {
	                matched = true; break;
                }
            }
        }
        if (!matched) {
            continue;
        }
        ++total;
        if (total <= offset || (int)results.size() >= limit) {
            continue;
        }
        json result = projectAgentRow(row, fields);
        if (search && (domain == "source" || domain == "cache") && (before || after)) {
            result["context"] = json::array();
            for (int j = std::max(0, i - before); j <= std::min((int)rows.size() - 1, i + after); ++j) {
                if (j != i && rows[j].at("file") == row.at("file")) {
                    result["context"].push_back(projectAgentRow(rows[j], fields));
                }
            }
        }
        results.push_back(std::move(result));
    }
    return {{"offset", offset}, {"limit", limit}, {"total", total}, {"rows", std::move(results)}};
}

NAMESPACE_END(gpp)
