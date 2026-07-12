module;

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
            return "name:" + it->template get<std::string>();
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
    JsonT referenceTargetToJson(const SentencePosition& target) {
        return JsonT{
            {"file", target.file},
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

    void normalizeRepeatedBlockReferences(RepeatedBlockReferenceMap& references) {
        std::vector<SentencePosition> selfReferences;
        for (auto& [target, source] : references.targetToSourceMap) {
            absl::flat_hash_set<SentencePosition> visited;
            SentencePosition root = source;
            while (true) {
                if (!visited.insert(root).second) {
                    break;
                }
                const auto it = references.targetToSourceMap.find(root);
                if (it == references.targetToSourceMap.end()) {
                    break;
                }
                root = it->second;
            }
            source = root;
            if (target == source) {
                selfReferences.push_back(target);
            }
        }
        for (const SentencePosition& target : selfReferences) {
            references.targetToSourceMap.erase(target);
        }

        references.sourceToTargetsMap.clear();
        for (const auto& [target, source] : references.targetToSourceMap) {
            if (target == source) {
                continue;
            }
            std::vector<SentencePosition>& targets = references.sourceToTargetsMap[source];
            if (!std::ranges::contains(targets, target)) {
                targets.push_back(target);
            }
        }
    }
}

RepeatedBlockReferenceMap buildRepeatedBlockReferenceMap(
    const std::vector<std::pair<fs::path, ordered_json>>& filesWithData,
    int minBlockSize
) {
    RepeatedBlockReferenceMap references;

    std::vector<int> tokens;
    std::vector<SentencePosition> positions;
    absl::flat_hash_map<std::string, int> tokenIds;
    int nextTokenId = 1;

    for (const auto& [relFilePath, data] : filesWithData) {
        for (const auto& [index, item] : data | std::views::enumerate) {
            const std::string sentenceKey = buildRepeatedBlockSentenceKey(item);
            const auto [it, inserted] = tokenIds.emplace(sentenceKey, nextTokenId);
            if (inserted) {
                ++nextTokenId;
            }
            tokens.push_back(it->second);
            positions.push_back({ wide2Ascii(relFilePath), (int)index });
        }
    }

    const std::vector<RepeatedBlockOccurrence> occurrences = collectRepeatedBlockOccurrences(tokens, minBlockSize);
    if (occurrences.size() < 2) {
        return references;
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
            const SentencePosition source = positions[sourceStart + offset];
            const SentencePosition target = positions[occurrence.start + offset];
            references.targetToSourceMap[target] = source;
            references.sourceToTargetsMap[source].push_back(target);
            referencedPositions.insert(occurrence.start + offset);
        }
    }

    normalizeRepeatedBlockReferences(references);
    return references;
}

void addReferenceInfoToInputJson(
    const fs::path& relFilePath,
    ordered_json& data,
    const RepeatedBlockReferenceMap& references
) {
    for (auto [index, item] : data | std::views::enumerate) {
        const SentencePosition target{ wide2Ascii(relFilePath), (int)index };
        if (const auto it = references.targetToSourceMap.find(target); it != references.targetToSourceMap.end()) {
            item["_gpp_ref_to"] = referenceTargetToJson<ordered_json>(it->second);
        }
        if (const auto it = references.sourceToTargetsMap.find(target); it != references.sourceToTargetsMap.end()) {
            ordered_json refs = ordered_json::array();
            for (const SentencePosition& refBy : it->second) {
                refs.push_back(referenceTargetToJson<ordered_json>(refBy));
            }
            item["_gpp_ref_by"] = std::move(refs);
        }
    }
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
    const std::string currentText = getNameString(s) + s->orig + s->preproc;

    std::string prevText = "None";
    std::string nextText = "None";
    if (s->prev) {
        prevText = getNameString(s->prev) + s->prev->orig + s->prev->preproc;
    }
    if (s->next) {
        nextText = getNameString(s->next) + s->next->orig + s->next->preproc;
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
            if (current->transCompleted) {
                const std::string name = current->nameType == NameType::None ? "null" : getNameString(current);
                contextLines.push_back(std::format("{}\t{}\t{}", name, current->pretrans, current->index));
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
            if (current->transCompleted) {
                contextLines.push_back(std::format("{}\t{}", current->pretrans, current->index));
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = "DST\tID\n" + (contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>());
    }
    break;

    case TransEngine::ForGalJson:
    {
        json historyJson = json::array();
        const Sentence* current = batch[0]->prev;
        const int limit = contextHistorySize * 2;
        for (int i = 0; current && (int)historyJson.size() < contextHistorySize && i < limit; ++i) {
            if (current->transCompleted) {
                json item;
                item["id"] = current->index;
                if (current->nameType != NameType::None) {
                    item["name"] = getNameString(current);
                }
                item["dst"] = current->pretrans;
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
            if (current->transCompleted) {
                if (current->nameType != NameType::None) {
                    contextLines.push_back(std::format("{}:::::{}", getNameString(current), current->pretrans)); // :::::
                }
                else {
                    contextLines.push_back(current->pretrans);
                }
            }
            current = current->prev;
        }
        if (contextLines.empty()) return {};
        history = contextLines | std::views::reverse | std::views::join_with('\n') | std::ranges::to<std::string>();
    }
    break;

    default:
        throw std::runtime_error(gppTr("buildContextHistory", "未知的 PromptType").toStdString());
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
        for (Sentence* se : batchToTransThisRound) {
            const std::string name = se->nameType == NameType::None ? "null" : getNameString(se);
            inputBlock += std::format("{}\t{}\t{}\n", name, se->preproc, se->index);
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::ForNovelTsv:
    {
        for (Sentence* se : batchToTransThisRound) {
            inputBlock += std::format("{}\t{}\n", se->preproc, se->index);
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::ForGalJson:
    {
        for (Sentence* se : batchToTransThisRound) {
            json item;
            item["id"] = se->index;
            if (se->nameType != NameType::None) {
                item["name"] = getNameString(se);
            }
            item["src"] = se->preproc;
            inputBlock += item.dump() + "\n";
            if (id2SentenceMap != nullptr) {
                (*id2SentenceMap)[se->index] = se;
            }
        }
    }
    break;

    case TransEngine::Sakura:
    {
        for (Sentence* se : batchToTransThisRound) {
            if (se->nameType != NameType::None) {
                inputBlock += std::format("{}:::::{}\n", getNameString(se), se->preproc);
            }
            else {
                inputBlock += se->preproc + "\n";
            }
        }
        if (!inputBlock.empty()) {
            inputBlock.pop_back(); // Sakura 引擎需要移除最后一个换行符？大概
        }
    }
    break;

    default:
        throw std::runtime_error(gppTr("fillBlockAndMap", "内部错误: 不支持的 TransEngine 用于构建输入").toStdString());
    }
}

std::string limitLogLines(std::string_view text, int maxLines, std::string_view tail) {
    int lineCount = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\n') {
            continue;
        }
        ++lineCount;
        if (lineCount == maxLines && i + 1 < text.size()) {
            std::string result(text.substr(0, i + 1));
            result += tail;
            if (!result.ends_with('\n')) {
                result += '\n';
            }
            return result;
        }
    }
    return std::string(text);
}

int parseContent(std::string& content, std::span<Sentence*> batchToTransThisRound,
    const absl::flat_hash_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
    std::string& rollingContext, TransEngine transEngine, bool showRollingContext, bool retransAllWhenFail)
{
    int parsedCount = 0;

    if (size_t pos = content.find("</think>"); pos != std::string::npos) {
        content = content.substr(pos + 8);
    }
    else if (pos = content.find("<end_think>"); pos != std::string::npos) {
        content = content.substr(pos + 11);
    }

    {
        static jpc::Regex rollingContextRegex{ R"(<rolling_context>\n*([\S\s]*?)\n*</rolling_context>)", defaultRegCompileModifier };
        jpc::VecNum vecNum;
        jpcre2::VecOff vecOff;
        jpc::RegexMatch rm(&rollingContextRegex);
        rm.setSubject(&content).setNumberedSubstringVector(&vecNum).setMatchStartOffsetVector(&vecOff);
        if (rm.match() > 0 && vecNum.size() > 0 && vecNum[0].size() > 1) {
            rollingContext = std::move(replaceStrInplace(vecNum[0][1], "<ORIGINAL>", rollingContext));
            if (rollingContext.contains("<ORIGINAL>")) {
                rollingContext.clear();
            }
            else {
                rollingContext = truncateUtf8Prefix(rollingContext, 256);
            }

            if (!showRollingContext && vecNum.size() == vecOff.size()) {
	            for (const auto& [matchedOff, matchedVec] :
                    std::views::zip(vecOff, vecNum) | std::views::reverse)
                {
                    content.erase(matchedOff, matchedVec.front().length());
	            }
            }
        }
        else {
            rollingContext.clear();
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
                if (const auto it = id2SentenceMap.find(id);
                    it != id2SentenceMap.end() && !it->second->transCompleted)
                {
                    it->second->pretrans = parts[1];
                    it->second->translatedBy = modelName;
                    it->second->transCompleted = true;
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
                if (const auto it = id2SentenceMap.find(id);
                    it != id2SentenceMap.end() && !it->second->transCompleted)
                {
                    it->second->pretrans = parts[0];
                    it->second->translatedBy = modelName;
                    it->second->transCompleted = true;
                    ++parsedCount;
                }
            }
            catch (...) { }
        }
    }
    break;

    case TransEngine::ForGalJson:
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
                if (const auto it = id2SentenceMap.find(id);
                    it != id2SentenceMap.end() && !it->second->transCompleted)
                {
                    it->second->pretrans = dst;
                    it->second->translatedBy = modelName;
                    it->second->transCompleted = true;
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

            currentSentence->pretrans = translatedLine;
            currentSentence->translatedBy = modelName;
            currentSentence->transCompleted = true;
            ++parsedCount;
        }
    }
    break;

    default:
        throw std::runtime_error(gppTr("parseContent", "内部错误: 不支持的 TransEngine 用于解析输出").toStdString());
    }

    if (retransAllWhenFail && parsedCount != batchToTransThisRound.size()) {
        for (Sentence* se : batchToTransThisRound | std::views::filter([](const auto& s) { return s->transCompleted; })) {
            se->pretrans.clear();
            se->translatedBy.clear();
            se->transCompleted = false;
	    }
    }
    return parsedCount;
}

void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
    const fs::path& outputCacheDir, const fs::path& outputDir, std::shared_ptr<spdlog::logger>& logger) {

    ordered_json combinedJson = ordered_json::array();

    std::ifstream ifs;
    logger->debug(gppTr("combineOutputFiles", "开始合并文件: %1")
        .arg(wide2Ascii(originalRelFilePath))
        .toStdString());

    std::vector<fs::path> partPaths = splitFileParts | std::views::keys | std::ranges::to<std::vector>();

    std::ranges::sort(partPaths, [&](const fs::path& a, const fs::path& b)
        {
            return getSplittedFileIndex(a.wstring()) < getSplittedFileIndex(b.wstring());
        });

    for (const auto& relPartPath : partPaths) {
        if (const fs::path partPath = outputCacheDir / relPartPath; fs::exists(partPath)) {
            const ordered_json partData = parseOrderedJson(partPath, ifs);
            combinedJson.insert(combinedJson.end(), partData.begin(), partData.end());
        }
        else {
            throw std::runtime_error(gppTr("combineOutputFiles", "试图合并 %1 时出错，缺少文件 %2")
                .arg(wide2Ascii(originalRelFilePath))
                .arg(wide2Ascii(partPath))
                .toStdString());
        }
    }

    const fs::path finalOutputPath = outputDir / originalRelFilePath;
    atomicOutputFile(finalOutputPath, combinedJson.dump(2));
    logger->info(gppTr("combineOutputFiles", "文件 %1 合并完成，已保存到 %2")
        .arg(wide2Ascii(originalRelFilePath))
        .arg(wide2Ascii(finalOutputPath))
        .toStdString());
}


bool hasRetranslKey(const std::vector<CheckSeCondNormalFunc>& retranslKeys, const json& item, const Sentence& currentSe) {
    if (retranslKeys.empty()) {
        return false;
    }

    Sentence probeSentence = currentSe;
    if (probeSentence.nameType == NameType::Single) {
        if (const auto jit = item.find("name_translated"); jit != item.end()) {
            jit->get_to(probeSentence.nameTrans);
        }
    }
    else if (probeSentence.nameType == NameType::Multiple) {
        if (const auto jit = item.find("names_translated"); jit != item.end()) {
            jit->get_to(probeSentence.namesTrans);
        }
    }
    if (const auto jit = item.find("problems"); jit != item.end()) {
        jit->get_to(probeSentence.problems);
    }
    if (const auto jit = item.find("translated_raw_text"); jit != item.end()) {
        jit->get_to(probeSentence.pretrans);
    }
    if (const auto jit = item.find("other_info"); jit != item.end()) {
        jit->get_to(probeSentence.otherInfo);
    }
    if (const auto jit = item.find("translated_by"); jit != item.end()) {
        jit->get_to(probeSentence.translatedBy);
    }
    if (const auto jit = item.find("translated_view_text"); jit != item.end()) {
        jit->get_to(probeSentence.transview);
    }

    return std::ranges::any_of(retranslKeys, [&](const CheckSeCondNormalFunc& key)
        {
            return key(&probeSentence);
        });
}

void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath) {
    json cacheJson = json::array();
    for (const auto& se : allSentences) {
        if (!se.transCompleted) {
            continue;
        }
        json cacheObj;
        cacheObj["index"] = se.index;
        if (se.nameType == NameType::Single) {
            cacheObj["name"] = se.name;
            cacheObj["name_translated"] = se.nameTrans;
        }
        else if (se.nameType == NameType::Multiple) {
            cacheObj["names"] = se.names;
            cacheObj["names_translated"] = se.namesTrans;
        }
        cacheObj["original_text"] = se.orig;
        if (!se.otherInfo.empty()) {
            cacheObj["other_info"] = se.otherInfo;
        }
        cacheObj["pre_processed_text"] = se.preproc;
        cacheObj["translated_raw_text"] = se.pretrans;
        if (!se.problems.empty()) {
            cacheObj["problems"] = se.problems;
        }
        cacheObj["translated_by"] = se.translatedBy;
        cacheObj["translated_view_text"] = se.transview;
        sentenceReferenceInfoToItem(cacheObj, se, true);
        cacheJson.push_back(std::move(cacheObj));
    }
    atomicOutputFile(cachePath, cacheJson.dump(2));
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
