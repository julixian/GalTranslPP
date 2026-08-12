module;

#include "GPPMacros.hpp"
#include <toml.hpp>

export module NormalJsonTranslatorHelperTool;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    struct RepeatedBlockReferenceMap {
        absl::flat_hash_map<SentencePosition, SentencePosition> targetToSourceMap;
        absl::flat_hash_map<SentencePosition, std::vector<SentencePosition>> sourceToTargetsMap;
    };

    template <typename JsonT>
    bool isRefPendingFromItem(const JsonT& item) {
        return item.value("_gpp_ref_pending", false);
    }

    template <typename JsonT>
    std::optional<SentencePosition> getRefToFromItem(const JsonT& item) {
        const auto it = item.find("_gpp_ref_to");
        if (it == item.end() || !it->is_object()) {
            return std::nullopt;
        }
        std::string file = it->value("file", "");
        const int index = it->value("index", -1);
        if (file.empty() || index < 0) {
            return std::nullopt;
        }
        return SentencePosition{ std::move(file), index };
    }

    template <typename JsonT>
    std::vector<SentencePosition> getRefByFromItem(const JsonT& item) {
        std::vector<SentencePosition> refs;
        const auto it = item.find("_gpp_ref_by");
        if (it == item.end() || !it->is_array()) {
            return refs;
        }
        for (const auto& refItem : *it) {
            if (!refItem.is_object()) {
                continue;
            }
            std::string file = refItem.value("file", "");
            const int index = refItem.value("index", -1);
            if (!file.empty() && index >= 0) {
                refs.push_back({ std::move(file), index });
            }
        }
        return refs;
    }

    template <typename JsonT>
    void itemReferenceInfoToSentence(const JsonT& item, Sentence& se, bool includePending) {
        se.ref = getRefToFromItem(item);
        se.refBy = getRefByFromItem(item);
        if (includePending) {
            se.isRefPending = isRefPendingFromItem(item);
        }
    }

    template <typename JsonT>
    void sentenceReferenceInfoToItem(JsonT& item, const Sentence& se, bool includePending) {
        if (se.ref.has_value()) {
            item["_gpp_ref_to"] = JsonT{
                {"file", se.ref->file},
                {"index", se.ref->index}
            };
        }
        if (!se.refBy.empty()) {
            JsonT refBy = JsonT::array();
            for (const auto& ref : se.refBy) {
                refBy.push_back({ {"file", ref.file}, {"index", ref.index} });
            }
            item["_gpp_ref_by"] = std::move(refBy);
        }
        if (includePending && se.isRefPending) {
            item["_gpp_ref_pending"] = true;
        }
    }

    template <typename JsonT>
    void eraseItemReferenceInfo(JsonT& item, bool includePending) {
        item.erase("_gpp_ref_to");
        item.erase("_gpp_ref_by");
        if (includePending) {
            item.erase("_gpp_ref_pending");
        }
    }

    RepeatedBlockReferenceMap buildRepeatedBlockReferenceMap(
        const std::vector<std::pair<fs::path, ordered_json*>>& filesWithData,
        int minBlockSize
    );

    void addReferenceInfoToInputJson(
        const fs::path& relFilePath,
        ordered_json& data,
        const RepeatedBlockReferenceMap& references
    );

    std::string generateCacheKey(const Sentence& s);
    std::string generateCacheKey(const json& jsonArr, size_t i);

    std::string buildContextHistory(std::span<Sentence*> batch, TransEngine transEngine, int contextHistorySize, int maxChars);
    std::string limitLogLines(std::string_view text, int maxLines, std::string_view tail = ".........");
    void fillBlockAndMap(
        std::span<Sentence*> batchToTransThisRound,
        std::string& inputBlock,
        TransEngine transEngine,
        absl::flat_hash_map<int, Sentence*>* id2SentenceMap = nullptr
    );

    std::string makeTransby(std::string_view apikey, std::string_view modelName);
    int parseContent(std::string& content, std::span<Sentence*> batchToTransThisRound, const absl::flat_hash_map<int, Sentence*>& id2SentenceMap,
        const std::string& transby, std::string& rollingContext, TransEngine transEngine, bool showRollingContext, bool retransAllWhenFail);

    void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
        const fs::path& outputCacheDir, const fs::path& outputDir, const std::shared_ptr<spdlog::logger>& logger);

    bool hasRetranslKey(const std::vector<CheckSeCondNormalFunc>& retranslKeys, const json& item, const Sentence& currentSentence);

    void saveTranslCache(
        const std::vector<Sentence>& sentences,
        const fs::path& cachePath,
        const fs::path& relInputPath,
        absl::flat_hash_map<fs::path, json>& savedTranslCacheMap,
        std::shared_mutex& transCacheMutex);

    std::vector<ordered_json> splitJsonArrayNum(ordered_json originalData, int chunkSize);
    std::vector<ordered_json> splitJsonArrayEqual(ordered_json originalData, int numParts);

    int getSplittedFileIndex(const std::wstring& path);
    int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2);
}
