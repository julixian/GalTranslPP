module;

#include "GPPMacros.hpp"

module NormalJsonTranslator;

import boost;
import NormalJsonTranslatorHelperTool;
import Tool;

NAMESPACE_BEGIN(gpp)

namespace fs = std::filesystem;

NormalJsonTranslatorTransAgent::SourceFileView NormalJsonTranslatorTransAgent::buildSourceFileView(
    const ordered_json& data, const std::function<void(Sentence*)>& preProcessFunc)
{
    std::vector<Sentence> sentences;
    sentences.reserve(data.size());
    for (const auto& [index, item] : data | std::views::enumerate) {
        Sentence se;
        se.index = (int)index;
        if (auto jit = item.find("name"); jit != item.end()) {
            se.nameType = NameType::Single;
            jit->get_to(se.name);
        }
        else if (jit = item.find("names"); jit != item.end()) {
            se.nameType = NameType::Multiple;
            jit->get_to(se.names);
        }
        item["message"].get_to(se.orig);
        sentences.push_back(std::move(se));
    }
    for (auto [se1, se2] : std::views::adjacent<2>(sentences)) {
        se1.next = &se2;
        se2.prev = &se1;
    }
    for (Sentence& se : sentences) {
        preProcessFunc(&se);
    }

    SourceFileView fileView;
    fileView.lines.reserve(sentences.size());
    for (const Sentence& se : sentences) {
        fileView.lines.push_back({
            .id = se.index,
            .speaker = getNameString(se),
            .sourceText = se.preproc
        });
    }
    return fileView;
}

const NormalJsonTranslatorTransAgent::SourceFileView* NormalJsonTranslatorTransAgent::findSourceFileView(
    const fs::path& relPath) const
{
    const auto it = m_sourceFileViews.find(relPath);
    if (it == m_sourceFileViews.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<int> NormalJsonTranslatorTransAgent::getSourceFileLineCount(const fs::path& relPath) const
{
    const auto* sourceView = findSourceFileView(relPath);
    if (sourceView == nullptr) {
        return std::nullopt;
    }
    return (int)sourceView->lines.size();
}

absl::flat_hash_map<int, json> NormalJsonTranslatorTransAgent::loadCacheDstMap(const fs::path& targetRelPath) const
{
    absl::flat_hash_map<int, json> cacheMap;
    const fs::path cachePath = m_transCacheDir / targetRelPath;
    std::shared_lock<std::shared_mutex> lock(m_transCacheMutex);
    json cacheJson;
    if (const auto it = m_savedTranslCacheMap.find(targetRelPath); it != m_savedTranslCacheMap.end()) {
        cacheJson = it->second;
    }
    else if (fs::exists(cachePath)) {
        cacheJson = parseJson(cachePath);
    }
    else {
        return cacheMap;
    }
    for (const auto& item : cacheJson) {
        const int index = item.value("index", -1);
        if (index >= 0) {
            cacheMap[index] = item;
        }
    }
    return cacheMap;
}

std::optional<int> NormalJsonTranslatorTransAgent::parseJsonInt(const json& object)
{
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

int NormalJsonTranslatorTransAgent::sanitizeToolLimit(int requested) const
{
    if (requested <= 0) {
        return m_agentSearchResultLimit;
    }
    return std::min(requested, m_agentSearchResultLimit);
}

json NormalJsonTranslatorTransAgent::listFiles(const json& arguments) const
{
    const fs::path nativeSpec = ascii2Wide(arguments.value("spec", ""));
    const int offset = std::max(0, arguments.value("offset", 0));
    const int limit = sanitizeToolLimit(arguments.value("limit", 0));
    json files = json::array();

    int matchCount = 0;
    for (const fs::path& relFile : m_knownRelFiles) {
        if (!nativeSpec.empty()) {
            if (!boost::algorithm::icontains(relFile.native(), nativeSpec.native()) &&
                !matchFileSpec(relFile.native(), nativeSpec.native()))
            {
                continue;
            }
        }
        ++matchCount;
        if (matchCount <= offset || (int)files.size() >= limit) {
            continue;
        }

        json fileInfo = {{"file", wide2Ascii(relFile)}};
        if (const auto lineCount = getSourceFileLineCount(relFile)) {
            fileInfo["lines"] = *lineCount;
        }
        files.push_back(std::move(fileInfo));
    }
    return {
        {"files", std::move(files)},
        {"offset", offset},
        {"limit", limit},
        {"total", matchCount}
    };
}

json NormalJsonTranslatorTransAgent::readProjectNote() const
{
    if (!m_agentProjectNotePath) {
        return {{"available", false}, {"file", nullptr}, {"content", ""}};
    }

    const fs::path& path = *m_agentProjectNotePath;
    std::error_code ec;
    const fs::path relPath = fs::relative(path, m_projectDir, ec);
    const std::string file = wide2Ascii(ec ? path.filename() : relPath);
    if (!fs::exists(path)) {
        return {{"available", false}, {"file", file}, {"content", ""}};
    }

    std::ifstream ifs(path, std::ios::binary);
    const std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return {{"file", file}, {"content", content}};
}

json NormalJsonTranslatorTransAgent::projectRow(const json& row, const json& fields)
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

// Agent 的读、搜索和备注工具；协议层负责回填消息的形式。
json NormalJsonTranslatorTransAgent::runReadTool(const fs::path& relInputPath, const std::string& name, const json& arguments)
{
    if (name == "list_files") {
        return listFiles(arguments);
    }
    if (name == "read_file_note") {
        const std::string file = arguments.value("file", "");
        const fs::path targetRelPath = file.empty() ? relInputPath : fs::path(ascii2Wide(file));
        return {{"file", wide2Ascii(targetRelPath)}, {"note", loadFileNote(targetRelPath)}};
    }
    if (name == "read_project_note") {
        return readProjectNote();
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
            row["dst"] = row.value("dst", "");
            rows.push_back(std::move(row));
        }
    }
    else {
        throw std::runtime_error("Unknown tool: " + name);
    }

    const int offset = std::max(0, arguments.value("offset", 0));
    const int limit = sanitizeToolLimit(arguments.value("limit", 0));
    const int before = std::clamp(arguments.value("context_before", 0), 0, m_agentContextLinesLimit);
    const int after = std::clamp(arguments.value("context_after", 0), 0, m_agentContextLinesLimit);
    const json ids = arguments.value("ids", json::array());
    const json fields = arguments.value("fields", json::array());
    const json matchFields = arguments.value("match_fields", json::array());
    const std::string query = arguments.value("query", "");
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
                matched = entry.value().is_string()
                    ? boost::algorithm::icontains(entry.value().get_ref<const std::string&>(), query)
                    : boost::algorithm::icontains(entry.value().dump(), query);
                if (matched) {
                    break;
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
        json result = projectRow(row, fields);
        if (search && (domain == "source" || domain == "cache") && (before || after)) {
            result["context"] = json::array();
            for (int j = std::max(0, i - before); j <= std::min((int)rows.size() - 1, i + after); ++j) {
                if (j != i && rows[j].at("file") == row.at("file")) {
                    result["context"].push_back(projectRow(rows[j], fields));
                }
            }
        }
        results.push_back(std::move(result));
    }
    return {{"offset", offset}, {"limit", limit}, {"total", total}, {"rows", std::move(results)}};
}

NAMESPACE_END(gpp)
