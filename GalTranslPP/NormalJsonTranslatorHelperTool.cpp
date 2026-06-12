module;

#define PCRE2_HEADERS
#include "GPPMacros.hpp"
#include <toml.hpp>

module NormalJsonTranslatorHelperTool;

import Tool;
namespace fs = std::filesystem;

namespace
{

struct RepeatedBlockOccurrence {
    int start = 0;
    int length = 0;
};

template <typename JsonT>
std::string buildRepeatedBlockSpeakerKey(const JsonT& item) {
    if (auto it = item.find("name"); it != item.end()) {
        return "name:" + it->get<std::string>();
    }
    if (auto it = item.find("names"); it != item.end()) {
        return "names:" + it->dump();
    }
    return "none:";
}

template <typename JsonT>
std::string buildRepeatedBlockSentenceKey(const JsonT& item) {
    return buildRepeatedBlockSpeakerKey(item) + "\nmessage:" + item.value("message", "");
}

template <typename JsonT>
JsonT referenceTargetToJson(const RepeatedBlockReferenceTarget& target) {
    return JsonT{
        {"file", wide2Ascii(target.file)},
        {"index", target.index}
    };
}

std::vector<int> buildSuffixArray(const std::vector<int>& tokens) {
    const int n = (int)tokens.size();
    std::vector<int> suffixArray(n);
    std::iota(suffixArray.begin(), suffixArray.end(), 0);
    if (n <= 1) {
        return suffixArray;
    }

    std::vector<int> rank = tokens;
    std::vector<int> nextRank(n);
    for (int k = 1;; k <<= 1) {
        std::ranges::sort(suffixArray, [&](int lhs, int rhs)
            {
                if (rank[lhs] != rank[rhs]) {
                    return rank[lhs] < rank[rhs];
                }
                const int lhsNext = lhs + k < n ? rank[lhs + k] : -1;
                const int rhsNext = rhs + k < n ? rank[rhs + k] : -1;
                return lhsNext < rhsNext;
            });

        nextRank[suffixArray.front()] = 0;
        for (int i = 1; i < n; ++i) {
            const int prev = suffixArray[i - 1];
            const int current = suffixArray[i];
            const bool different =
                rank[prev] != rank[current] ||
                (prev + k < n ? rank[prev + k] : -1) != (current + k < n ? rank[current + k] : -1);
            nextRank[current] = nextRank[prev] + (different ? 1 : 0);
        }
        rank.swap(nextRank);
        if (rank[suffixArray.back()] == n - 1) {
            break;
        }
    }
    return suffixArray;
}

std::vector<int> buildLcpArray(const std::vector<int>& tokens, const std::vector<int>& suffixArray) {
    const int n = (int)tokens.size();
    std::vector<int> rank(n);
    for (int i = 0; i < n; ++i) {
        rank[suffixArray[i]] = i;
    }

    std::vector<int> lcp(std::max(0, n - 1));
    int h = 0;
    for (int i = 0; i < n; ++i) {
        const int r = rank[i];
        if (r == 0) {
            continue;
        }
        const int j = suffixArray[r - 1];
        while (i + h < n && j + h < n && tokens[i + h] == tokens[j + h]) {
            ++h;
        }
        lcp[r - 1] = h;
        if (h > 0) {
            --h;
        }
    }
    return lcp;
}

std::vector<RepeatedBlockOccurrence> collectRepeatedBlockOccurrences(const std::vector<int>& tokens, int minBlockSize) {
    std::vector<RepeatedBlockOccurrence> occurrences;
    if ((int)tokens.size() < minBlockSize * 2) {
        return occurrences;
    }

    const std::vector<int> suffixArray = buildSuffixArray(tokens);
    const std::vector<int> lcp = buildLcpArray(tokens, suffixArray);

    absl::flat_hash_set<int> starts;
    for (const auto& [i, commonLength] : lcp | std::views::enumerate) {
        if (commonLength < minBlockSize) {
            continue;
        }
        const size_t suffixIndex = (size_t)i;
        const int lhs = suffixArray[suffixIndex];
        const int rhs = suffixArray[suffixIndex + 1];
        const int length = std::min(commonLength, std::abs(lhs - rhs));
        if (length < minBlockSize) {
            continue;
        }
        if (starts.insert(lhs).second) {
            occurrences.push_back({ lhs, length });
        }
        if (starts.insert(rhs).second) {
            occurrences.push_back({ rhs, length });
        }
    }

    std::ranges::sort(occurrences, [](const RepeatedBlockOccurrence& a, const RepeatedBlockOccurrence& b)
        {
            if (a.start != b.start) {
                return a.start < b.start;
            }
            return a.length > b.length;
        });
    return occurrences;
}

} // namespace

RepeatedBlockPlan analyzeRepeatedBlocks(
    const std::vector<std::pair<fs::path, ordered_json>>& files,
    int minBlockSize
) {
    RepeatedBlockPlan plan;
    if (minBlockSize <= 1) {
        minBlockSize = 2;
    }

    std::vector<int> tokens;
    std::vector<RepeatedBlockReferenceTarget> positions;
    absl::flat_hash_map<std::string, int> tokenIds;
    int nextTokenId = 1;

    for (const auto& [relFilePath, data] : files) {
        for (const auto& [index, item] : data | std::views::enumerate) {
            const std::string key = buildRepeatedBlockSentenceKey(item);
            const auto [it, inserted] = tokenIds.emplace(key, nextTokenId);
            if (inserted) {
                ++nextTokenId;
            }
            tokens.push_back(it->second);
            positions.push_back({ relFilePath, (int)index });
        }
    }

    const std::vector<RepeatedBlockOccurrence> occurrences = collectRepeatedBlockOccurrences(tokens, minBlockSize);
    if (occurrences.size() < 2) {
        return plan;
    }

    std::vector<uint64_t> hashPrefix(tokens.size() + 1);
    std::vector<uint64_t> hashPower(tokens.size() + 1, 1);
    for (size_t i = 0; i < tokens.size(); ++i) {
        constexpr uint64_t hashBase = 11400714819323198485ull;
        hashPrefix[i + 1] = hashPrefix[i] * hashBase + (uint64_t)tokens[i] + 0x9E3779B97F4A7C15ull;
        hashPower[i + 1] = hashPower[i] * hashBase;
    }
    auto windowHash = [&](int start, int length)
        {
            return hashPrefix[start + length] - hashPrefix[start] * hashPower[length];
        };
    auto sameMinWindow = [&](int lhs, int rhs)
        {
            return std::ranges::equal(
                tokens.begin() + lhs,
                tokens.begin() + lhs + minBlockSize,
                tokens.begin() + rhs,
                tokens.begin() + rhs + minBlockSize
            );
        };

    absl::flat_hash_map<uint64_t, std::vector<int>> sourceByBlockHash;
    absl::flat_hash_set<int> referencedPositions;

    for (const RepeatedBlockOccurrence& occurrence : occurrences) {
        if (occurrence.start + minBlockSize > (int)tokens.size()) {
            continue;
        }
        std::vector<int>& sourceStarts = sourceByBlockHash[windowHash(occurrence.start, minBlockSize)];
        const auto sourceIt = std::ranges::find_if(sourceStarts, [&](int candidateStart)
            {
                return sameMinWindow(candidateStart, occurrence.start);
            });
        if (sourceIt == sourceStarts.end()) {
            sourceStarts.push_back(occurrence.start);
            continue;
        }

        const int sourceStart = *sourceIt;
        if (occurrence.start == sourceStart || std::abs(occurrence.start - sourceStart) < minBlockSize) {
            continue;
        }
        int length = 0;
        const int maxLength = std::min((int)tokens.size() - occurrence.start, (int)tokens.size() - sourceStart);
        while (length < maxLength && tokens[sourceStart + length] == tokens[occurrence.start + length]) {
            ++length;
        }
        length = std::min(length, std::abs(occurrence.start - sourceStart));
        if (length < minBlockSize) {
            continue;
        }

        bool overlapsExistingReference = false;
        for (int offset = 0; offset < length; ++offset) {
            if (referencedPositions.contains(occurrence.start + offset)) {
                overlapsExistingReference = true;
                break;
            }
        }
        if (overlapsExistingReference) {
            continue;
        }

        for (int offset = 0; offset < length; ++offset) {
            const RepeatedBlockReferenceTarget source = positions[sourceStart + offset];
            const RepeatedBlockReferenceTarget target = positions[occurrence.start + offset];
            plan.refToByTarget[target] = source;
            plan.refBySource[source].push_back(target);
            referencedPositions.insert(occurrence.start + offset);
        }
    }

    return plan;
}

void applyRepeatedBlockPlanToJson(
    const fs::path& relFilePath,
    ordered_json& data,
    const RepeatedBlockPlan& plan
) {
    for (auto [index, item] : data | std::views::enumerate) {
        eraseRepeatedBlockReferenceInfo(item);
        const RepeatedBlockReferenceTarget target{ relFilePath, (int)index };
        if (const auto it = plan.refToByTarget.find(target); it != plan.refToByTarget.end()) {
            item[std::string(repeatedBlockRefToKey)] = referenceTargetToJson<ordered_json>(it->second);
        }
        if (const auto it = plan.refBySource.find(target); it != plan.refBySource.end()) {
            ordered_json refs = ordered_json::array();
            for (const RepeatedBlockReferenceTarget& refBy : it->second) {
                refs.push_back(referenceTargetToJson<ordered_json>(refBy));
            }
            item[std::string(repeatedBlockRefByKey)] = std::move(refs);
        }
    }
}

bool isEscapedJsonQuote(const std::string& text, size_t pos) {
    if (pos == 0 || pos >= text.size()) {
        return false;
    }
    size_t slashCount = 0;
    for (size_t i = pos; i > 0;) {
        --i;
        if (text[i] != '\\') {
            break;
        }
        ++slashCount;
    }
    return slashCount % 2 == 1;
}

bool isLikelyJsonKeyPosition(const std::string& text, size_t keyPos) {
    if (keyPos == std::string::npos) {
        return false;
    }
    for (size_t i = keyPos; i > 0;) {
        --i;
        const unsigned char ch = (unsigned char)text[i];
        if (std::isspace(ch)) {
            continue;
        }
        return text[i] == '{' || text[i] == ',' || text[i] == '[';
    }
    return true;
}

bool isLikelyJsonStringSuffix(const std::string& text, size_t posAfterQuote) {
    for (size_t i = posAfterQuote; i < text.size(); ++i) {
        const unsigned char ch = (unsigned char)text[i];
        if (std::isspace(ch)) {
            continue;
        }
        return text[i] == ',' || text[i] == '}' || text[i] == ']';
    }
    return true;
}

size_t findLikelyJsonStringClosingQuote(const std::string& text, size_t openingQuotePos) {
    for (size_t pos = openingQuotePos + 1; pos < text.size(); ++pos) {
        if (text[pos] != '"' || isEscapedJsonQuote(text, pos)) {
            continue;
        }
        if (isLikelyJsonStringSuffix(text, pos + 1)) {
            return pos;
        }
    }
    return std::string::npos;
}

std::string lightRepairJsonText(const std::string& text) {
    if (text.empty()) {
        return text;
    }

    static constexpr std::array<std::string_view, 5> repairableFields = {
        "\"dst\":",
        "\"note\":",
        "\"rolling_context\":",
        "\"context\":",
        "\"reason\":"
    };

    std::string newText = text;
    for (const std::string_view& field : repairableFields) {
        size_t searchPos = 0;
        while (searchPos < newText.size()) {
            const size_t fieldPos = newText.find(field, searchPos);
            if (fieldPos == std::string::npos) {
                break;
            }
            searchPos = fieldPos + field.size();
            if (!isLikelyJsonKeyPosition(newText, fieldPos)) {
                continue;
            }

            size_t openingQuotePos = searchPos;
            while (openingQuotePos < newText.size() && std::isspace((unsigned char)newText[openingQuotePos])) {
                ++openingQuotePos;
            }
            if (openingQuotePos >= newText.size() || newText[openingQuotePos] != '"') {
                continue;
            }

            const size_t closingQuotePos = findLikelyJsonStringClosingQuote(newText, openingQuotePos);
            if (closingQuotePos == std::string::npos || closingQuotePos <= openingQuotePos) {
                continue;
            }

            std::string repairedValue;
            repairedValue.reserve(closingQuotePos - openingQuotePos);
            for (size_t pos = openingQuotePos + 1; pos < closingQuotePos; ++pos) {
                if (newText[pos] == '"' && !isEscapedJsonQuote(newText, pos)) {
                    repairedValue.push_back('\\');
                }
                repairedValue.push_back(newText[pos]);
            }

            const std::string_view originalValue = std::string_view(newText).substr(openingQuotePos + 1, closingQuotePos - openingQuotePos - 1);
            if (repairedValue != originalValue) {
                newText.replace(openingQuotePos + 1, closingQuotePos - openingQuotePos - 1, repairedValue);
                searchPos = openingQuotePos + 1 + repairedValue.size() + 1;
            }
        }
    }
    return newText;
}

int getSplittedFileIndex(const std::wstring& path) {
    const size_t pos1 = path.find_last_of(L'_') + 1;
    const size_t pos2 = path.find_last_of(L'.');
    return std::stoi(path.substr(pos1, pos2 - pos1));
}

/**
* @brief 根据句子的上下文生成唯一的缓存键，复刻 GalTransl 逻辑
*/
std::string generateCacheKey(const Sentence* s) {
    const std::string currentText = getNameString(s) + s->original_text + s->pre_processed_text;

    std::string prevText = "None";
    std::string nextText = "None";
    if (s->prev) {
        prevText = getNameString(s->prev) + s->prev->original_text + s->prev->pre_processed_text;
    }
    if (s->next) {
        nextText = getNameString(s->next) + s->next->original_text + s->next->pre_processed_text;
    }
    return prevText + currentText + nextText;
}

std::string generateCacheKey(const json& jsonArr, size_t i) {
    const auto& item = jsonArr[i];
    const std::string currentText = getNameString(item) + item.value("original_text", "") + item.value("pre_processed_text", "");

    std::string prevText = "None";
    std::string nextText = "None";
    if (i > 0) {
        const auto& lastItem = jsonArr[i - 1];
        prevText = getNameString(lastItem) + lastItem.value("original_text", "") + lastItem.value("pre_processed_text", "");
    }
    if (i + 1 < jsonArr.size()) {
        const auto& nextItem = jsonArr[i + 1];
        nextText = getNameString(nextItem) + nextItem.value("original_text", "") + nextItem.value("pre_processed_text", "");
    }
    return prevText + currentText + nextText;
}

std::string buildContextHistory(std::span<Sentence*> batch, TransEngine transEngine, int contextHistorySize, int maxChars) {
    if (batch.empty() || !batch[0]->prev) {
        return {};
    }

    std::string history;

    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                const std::string name = current->nameType == NameType::None ? "null" : getNameString(current);
                contextLines.push_back(std::format("{}\t{}\t{}", name, current->pre_translated_text, current->index));
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = "NAME\tDST\tID\n" + (contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>());
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        std::vector<std::string> contextLines;
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                contextLines.push_back(std::format("{}\t{}", current->pre_translated_text, current->index));
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = "DST\tID\n" + (contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>());
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        json historyJson = json::array();
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)historyJson.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                json item;
                item["id"] = current->index;
                if (current->nameType != NameType::None) {
                    item["name"] = getNameString(current);
                }
                item["dst"] = current->pre_translated_text;
                historyJson.push_back(std::move(item));
            }
            current = current->prev;
        }
        if (historyJson.empty()) return {};
        history = "```jsonline\n" + 
            (historyJson | std::views::reverse | std::views::transform([](const auto& item) { return item.dump(); })
            | std::views::join_with('\n') | std::ranges::to<std::string>()) 
    		    + "\n```";
    }
    break;

    case TransEngine::Sakura:
    {
        const Sentence* current = batch[0]->prev;
        std::vector<std::string> contextLines;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)contextLines.size() < contextHistorySize && i < limit; ++i) {
            if (current->complete) {
                if (current->nameType != NameType::None) {
                    contextLines.push_back(std::format("{}:::::{}", getNameString(current), current->pre_translated_text)); // :::::
                }
                else {
                    contextLines.push_back(current->pre_translated_text);
                }
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>();
    }
    break;

    default:
        throw std::runtime_error("未知的 PromptType");
    }

    return truncateUtf8Suffix(history, maxChars);
}

void fillBlockAndMap(
    std::span<Sentence*> batchToTransThisRound,
    std::string& inputBlock,
    TransEngine transEngine,
    absl::flat_hash_map<int, Sentence*>* id2SentenceMap
) {
    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        for (const auto& se : batchToTransThisRound) {
            const std::string name = se->nameType == NameType::None ? "null" : getNameString(se);
            inputBlock += std::format("{}\t{}\t{}\n", name, se->pre_processed_text, se->index);
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        for (const auto& se : batchToTransThisRound) {
            inputBlock += std::format("{}\t{}\n", se->pre_processed_text, se->index);
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        for (const auto& se : batchToTransThisRound) {
            json item;
            item["id"] = se->index;
            if (se->nameType != NameType::None) {
                item["name"] = getNameString(se);
            }
            item["src"] = se->pre_processed_text;
            inputBlock += item.dump() + "\n";
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::Sakura:
    {
        for (const auto& se : batchToTransThisRound) {
            if (se->nameType != NameType::None) {
                inputBlock += std::format("{}:::::{}\n", getNameString(se), se->pre_processed_text);
            }
            else {
                inputBlock += se->pre_processed_text + "\n";
            }
        }
        if (!inputBlock.empty()) {
            inputBlock.pop_back(); // 移除最后一个换行符
        }
    }
    break;

    default:
        throw std::runtime_error("不支持的 TransEngine 用于构建输入");
    }
}

int parseContent(std::string& content, std::span<Sentence*> batchToTransThisRound, const absl::flat_hash_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
    std::string& backgroudText, TransEngine transEngine, bool showBackgroundText, bool retransAllWhenFail) 
{
    int parsedCount = 0;

    if (size_t pos = content.find("</think>"); pos != std::string::npos) {
        content = content.substr(pos + 8);
    }
    else if (pos = content.find("<end_think>"); pos != std::string::npos) {
        content = content.substr(pos + 11);
    }

    {
        static jpc::Regex backgroundRegex{ R"(<background>\n*([\S\s]*?)\n*</background>)", defaultRegCompileModifier };
        jpc::VecNum vecNum;
        jpcre2::VecOff vecOff;
        jpc::RegexMatch rm(&backgroundRegex);
        rm.setSubject(&content).setNumberedSubstringVector(&vecNum).setMatchStartOffsetVector(&vecOff);
        if (rm.match() > 0 && vecNum.size() > 0 && vecNum[0].size() > 1) {

            backgroudText = std::move(replaceStrInplace(vecNum[0][1], "<ORIGINAL>", backgroudText));
            if (backgroudText.contains("<ORIGINAL>")) {
                backgroudText.clear();
            }
            else {
                backgroudText = truncateUtf8Prefix(backgroudText, 256);
            }

            if (!showBackgroundText && vecNum.size() == vecOff.size()) {
	            for (const auto& [matchedOff, matchedVec] : std::views::zip(vecOff, vecNum) | std::views::reverse) {
                    content.erase(matchedOff, matchedVec.front().length());
	            }
            }
        }
        else {
            backgroudText.clear();
        }
    }

    switch (transEngine)
    {
    case TransEngine::ForGalTsv:
    {
        size_t start = content.find("NAME\tDST\tID");
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitStringView(std::string_view(content).substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || line.contains("```")) {
                continue;
            }
            const auto parts = splitStringView(line, '\t');
            if (parts.size() < 3) {
                continue;
            }
            try {
                const int id = str2Int(parts[2]).value();
                if (const auto it = id2SentenceMap.find(id); it != id2SentenceMap.end() && !it->second->complete) {
                    it->second->pre_translated_text = parts[1];
                    it->second->translated_by = modelName;
                    it->second->complete = true;
                    ++parsedCount;
                }
            }
            catch (...) { }
        }
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        const size_t start = content.find("DST\tID"); // or DST\tID
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitStringView(std::string_view(content).substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || line.contains("```")) {
                continue;
            }
            const auto parts = splitStringView(line, '\t');
            if (parts.size() < 2) {
                continue;
            }
            try {
                const int id = str2Int(parts[1]).value();
                if (const auto it = id2SentenceMap.find(id); it != id2SentenceMap.end() && !it->second->complete) {
                    it->second->pre_translated_text = parts[0];
                    it->second->translated_by = modelName;
                    it->second->complete = true;
                    ++parsedCount;
                }
            }
            catch (...) { }
        }
    }
    break;

    case TransEngine::ForGalJson:
    case TransEngine::DeepseekJson:
    {
        const size_t start = std::min(content.find("{\"id\""), content.find("{\"dst\""));
        if (start == std::string::npos) {
            break;
        }

        const auto lines = splitStringView(std::string_view(content).substr(start), '\n');
        for (const auto& line : lines) {
            if (parsedCount == batchToTransThisRound.size()) {
                break;
            }
            if (line.empty() || !line.starts_with('{')) {
                continue;
            }
            try {
                json item = [&]()
	                {
                        try {
                            return json::parse(line);
                        }
                        catch (...) {
                            return json::parse(lightRepairJsonText(std::string(line)));
                        }
                    }();
                const int id = item["id"];
                const std::string dst = item["dst"].get<std::string>();
                if (const auto it = id2SentenceMap.find(id); it != id2SentenceMap.end() && !it->second->complete) {
                    it->second->pre_translated_text = dst;
                    it->second->translated_by = modelName;
                    it->second->complete = true;
                    ++parsedCount;
                }
            }
            catch (...) { }
        }
    }
    break;

    case TransEngine::Sakura:
    {
        auto lines = splitStringView(content, '\n');
        // 核心检查：行数是否匹配
        if (lines.size() != batchToTransThisRound.size()) {
            break;
        }

        for (auto [translatedLine, currentSentence] : std::views::zip(lines, batchToTransThisRound)) {
            // 尝试剥离说话人
            if (!currentSentence->name.empty()) {
                if (const size_t msgStart = translatedLine.find(":::::"); msgStart != std::string::npos) {
                    translatedLine = translatedLine.substr(msgStart + 5);
                }
            }

            currentSentence->pre_translated_text = translatedLine;
            currentSentence->translated_by = modelName;
            currentSentence->complete = true;
            ++parsedCount;
        }
    }
    break;

    default:
        throw std::runtime_error("不支持的 TransEngine 用于解析输出");
    }

    if (retransAllWhenFail && parsedCount != batchToTransThisRound.size()) {
        for (Sentence* se : batchToTransThisRound | std::views::filter([](const auto& s) { return s->complete; })) {
            se->pre_translated_text.clear();
            se->translated_by.clear();
            se->complete = false;
	    }
    }
    return parsedCount;
}

void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
    const fs::path& outputCacheDir, const fs::path& outputDir, std::shared_ptr<spdlog::logger>& logger) {

    ordered_json combinedJson = ordered_json::array();

    std::ifstream ifs;
    logger->debug("开始合并文件: {}", wide2Ascii(originalRelFilePath));

    std::vector<fs::path> partPaths = splitFileParts | std::views::keys | std::ranges::to<std::vector>();

    std::ranges::sort(partPaths, [&](const fs::path& a, const fs::path& b)
        {
            return getSplittedFileIndex(a.wstring()) < getSplittedFileIndex(b.wstring());
        });

    for (const auto& relPartPath : partPaths) {
        if (const fs::path partPath = outputCacheDir / relPartPath; fs::exists(partPath)) {
            try {
                ifs.open(partPath, std::ios::binary);
                ordered_json partData = ordered_json::parse(ifs);
                ifs.close();
                combinedJson.insert(combinedJson.end(), partData.begin(), partData.end());
            }
            catch (const json::exception& e) {
                ifs.close();
                logger->critical("合并文件 {} 时出错", wide2Ascii(partPath));
                throw std::runtime_error(e.what());
            }
        }
        else {
            throw std::runtime_error(std::format("试图合并 {} 时出错，缺少文件 {}", wide2Ascii(originalRelFilePath), wide2Ascii(partPath)));
        }
    }

    const fs::path finalOutputPath = outputDir / originalRelFilePath;
    createParent(finalOutputPath);
    std::ofstream ofs(finalOutputPath, std::ios::binary);
    ofs << combinedJson.dump(2);
    ofs.close();
    logger->info("文件 {} 合并完成，已保存到 {}", wide2Ascii(originalRelFilePath), wide2Ascii(finalOutputPath));
}


bool hasRetranslKey(const std::vector<CheckSeCondFunc>& retranslKeys, const json& item, const Sentence* currentSe) {
    if (retranslKeys.empty()) {
        return false;
    }

    Sentence se;
    if (item.contains("name")) {
        se.nameType = NameType::Single;
        se.name = item.at("name");
        se.name_preview = item.at("name_preview");
    }
    else if (item.contains("names")) {
        se.nameType = NameType::Multiple;
        se.names = item.at("names");
        se.names_preview = item.at("names_preview");
    }
    se.original_text = item.value("original_text", "");
    se.pre_processed_text = item.value("pre_processed_text", "");
    se.pre_translated_text = item.value("pre_translated_text", "");
    if (item.contains("problems")) {
        item.at("problems").get_to(se.problems);
    }
    if (item.contains("other_info")) {
        item.at("other_info").get_to(se.other_info);
    }
    se.translated_by = item.value("translated_by", "");
    se.translated_preview = item.value("translated_preview", "");
    se.prev = currentSe->prev;
    se.next = currentSe->next;

    return std::ranges::any_of(retranslKeys, [&](const CheckSeCondFunc& key)
        {
            return key(&se);
        });
}

void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath) {
    json cacheJson = json::array();
    for (const auto& se : allSentences) {
        if (!se.complete) {
            continue;
        }
        json cacheObj;
        cacheObj["index"] = se.index;
        if (se.nameType == NameType::Single) {
            cacheObj["name"] = se.name;
            cacheObj["name_preview"] = se.name_preview;
        }
        else if (se.nameType == NameType::Multiple) {
            cacheObj["names"] = se.names;
            cacheObj["names_preview"] = se.names_preview;
        }
        cacheObj["original_text"] = se.original_text;
        if (!se.other_info.empty()) {
            cacheObj["other_info"] = se.other_info;
        }
        cacheObj["pre_processed_text"] = se.pre_processed_text;
        cacheObj["pre_translated_text"] = se.pre_translated_text;
        if (!se.problems.empty()) {
            cacheObj["problems"] = se.problems;
        }
        cacheObj["translated_by"] = se.translated_by;
        cacheObj["translated_preview"] = se.translated_preview;
        writeRepeatedBlockReferenceInfo(cacheObj, se, true);
        cacheJson.push_back(std::move(cacheObj));
    }
    std::ofstream ofs(cachePath, std::ios::binary);
    ofs << cacheJson.dump(2);
    ofs.close();
}


std::vector<ordered_json> splitJsonArrayNum(const ordered_json& originalData, int chunkSize) {
    if (chunkSize <= 1 || originalData.empty()) {
        return { originalData };
    }
    std::vector<ordered_json> parts;
    parts.reserve((originalData.size() + chunkSize - 1) / chunkSize);
    const size_t totalSize = originalData.size();
    for (size_t i = 0; i < totalSize; i += chunkSize) {
        const size_t end = std::min(i + chunkSize, totalSize);
        parts.emplace_back(originalData.begin() + i, originalData.begin() + end);
    }
    return parts;
}


std::vector<ordered_json> splitJsonArrayEqual(const ordered_json& originalData, int numParts) {
    if (numParts == 1 || originalData.empty()) {
        return { originalData };
    }
    std::vector<ordered_json> parts;
    parts.reserve(numParts);
    const size_t totalSize = originalData.size();
    const size_t partSize = totalSize / numParts;
    const size_t remainder = totalSize % numParts;
    size_t currentIndex = 0;
    for (int i = 0; i < numParts; ++i) {
        const size_t currentPartSize = partSize + (i < remainder ? 1 : 0);
        if (currentPartSize == 0) {
            continue;
        }
        parts.emplace_back(originalData.begin() + currentIndex, originalData.begin() + currentIndex + currentPartSize);
        currentIndex += currentPartSize;
    }
    return parts;
}

int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2) {
    return getSplittedFileIndex(path1) - getSplittedFileIndex(path2);
}

json toml2Json(const toml::value& value) {
    if (value.is_table()) {
        json resultMap = json::object();
        for (const auto& [key, val] : value.as_table()) {
            resultMap[key] = toml2Json(val);
        }
        return resultMap;
    }
    else if (value.is_array()) {
        json resultVec = json::array();
        for (const auto& elem : value.as_array()) {
            resultVec.push_back(toml2Json(elem));
        }
        return resultVec;
    }
    else if (value.is_string()) {
        return value.as_string();
    }
    else if (value.is_integer()) {
        return value.as_integer();
    }
    else if (value.is_floating()) {
        return value.as_floating();
    }
    else if (value.is_boolean()) {
        return value.as_boolean();
    }
    throw std::runtime_error("不支持的 TOML 数据类型");
}

ordered_json toml2Json(const toml::ordered_value& value) {
    if (value.is_table()) {
        ordered_json resultMap = ordered_json::object();
        for (const auto& [key, val] : value.as_table()) {
            resultMap[key] = toml2Json(val);
        }
        return resultMap;
    }
    else if (value.is_array()) {
        ordered_json resultVec = ordered_json::array();
        for (const auto& elem : value.as_array()) {
            resultVec.push_back(toml2Json(elem));
        }
        return resultVec;
    }
    else if (value.is_string()) {
        return value.as_string();
    }
    else if (value.is_integer()) {
        return value.as_integer();
    }
    else if (value.is_floating()) {
        return value.as_floating();
    }
    else if (value.is_boolean()) {
        return value.as_boolean();
    }
    throw std::runtime_error("不支持的 TOML 数据类型");
}
