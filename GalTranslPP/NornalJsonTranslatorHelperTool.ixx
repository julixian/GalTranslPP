module;

#include "GPPMacros.hpp"
#include <toml.hpp>

export module NormalJsonTranslatorHelperTool;

import Tool;
import ITranslator;

namespace fs = std::filesystem;

export
{
    constexpr std::string_view repeatedBlockRefToKey = "_gpp_ref_to";
    constexpr std::string_view repeatedBlockRefByKey = "_gpp_ref_by";
    constexpr std::string_view repeatedBlockRefPendingKey = "_gpp_ref_pending";

    struct RepeatedBlockReferenceTarget {
        fs::path file;
        int index = -1;

        friend bool operator==(const RepeatedBlockReferenceTarget&, const RepeatedBlockReferenceTarget&) = default;
    };

    template <typename H>
    H AbslHashValue(H h, const RepeatedBlockReferenceTarget& target) {
        return H::combine(std::move(h), target.file, target.index);
    }

    struct RepeatedBlockPlan {
        absl::flat_hash_map<RepeatedBlockReferenceTarget, RepeatedBlockReferenceTarget> refToByTarget;
        absl::flat_hash_map<RepeatedBlockReferenceTarget, std::vector<RepeatedBlockReferenceTarget>> refBySource;
    };

    template <typename JsonT>
    bool isRepeatedBlockRefPendingCache(const JsonT& item) {
        return item.value(std::string(repeatedBlockRefPendingKey), false);
    }

    template <typename JsonT>
    std::optional<SentenceReference> getRepeatedBlockRefTo(const JsonT& item) {
        const auto it = item.find(std::string(repeatedBlockRefToKey));
        if (it == item.end() || !it->is_object()) {
            return std::nullopt;
        }
        const std::string file = it->value("file", "");
        const int index = it->value("index", -1);
        if (file.empty() || index < 0) {
            return std::nullopt;
        }
        return SentenceReference{ file, index };
    }

    template <typename JsonT>
    std::vector<SentenceReference> getRepeatedBlockRefBy(const JsonT& item) {
        std::vector<SentenceReference> refs;
        const auto it = item.find(std::string(repeatedBlockRefByKey));
        if (it == item.end() || !it->is_array()) {
            return refs;
        }
        for (const auto& refItem : *it) {
            if (!refItem.is_object()) {
                continue;
            }
            const std::string file = refItem.value("file", "");
            const int index = refItem.value("index", -1);
            if (!file.empty() && index >= 0) {
                refs.push_back({ file, index });
            }
        }
        return refs;
    }

    template <typename JsonT>
    void readRepeatedBlockReferenceInfo(const JsonT& item, Sentence& se) {
        se.repeatedBlockRefTo = getRepeatedBlockRefTo(item);
        se.repeatedBlockRefBy = getRepeatedBlockRefBy(item);
        se.repeatedBlockRefPending = isRepeatedBlockRefPendingCache(item);
    }

    template <typename JsonT>
    void writeRepeatedBlockReferenceInfo(JsonT& item, const Sentence& se, bool includePending) {
        if (se.repeatedBlockRefTo.has_value()) {
            item[std::string(repeatedBlockRefToKey)] = JsonT{
                {"file", se.repeatedBlockRefTo->file},
                {"index", se.repeatedBlockRefTo->index}
            };
        }
        if (!se.repeatedBlockRefBy.empty()) {
            JsonT refBy = JsonT::array();
            for (const auto& ref : se.repeatedBlockRefBy) {
                refBy.push_back({ {"file", ref.file}, {"index", ref.index} });
            }
            item[std::string(repeatedBlockRefByKey)] = std::move(refBy);
        }
        if (includePending && se.repeatedBlockRefPending) {
            item[std::string(repeatedBlockRefPendingKey)] = true;
        }
    }

    template <typename JsonT>
    void eraseRepeatedBlockReferenceInfo(JsonT& item) {
        item.erase(std::string(repeatedBlockRefToKey));
        item.erase(std::string(repeatedBlockRefByKey));
        item.erase(std::string(repeatedBlockRefPendingKey));
    }

    RepeatedBlockPlan analyzeRepeatedBlocks(
        const std::vector<std::pair<fs::path, ordered_json>>& files,
        int minBlockSize
    );

    void applyRepeatedBlockPlanToJson(
        const fs::path& relFilePath,
        ordered_json& data,
        const RepeatedBlockPlan& plan
    );

    std::string generateCacheKey(const Sentence* s);
    std::string generateCacheKey(const json& jsonArr, size_t i);

    std::string buildContextHistory(std::span<Sentence*> batch, TransEngine transEngine, int contextHistorySize, int maxChars);
    std::string lightRepairJsonText(const std::string& text);

    void fillBlockAndMap(
        std::span<Sentence*> batchToTransThisRound,
        std::string& inputBlock,
        TransEngine transEngine,
        absl::flat_hash_map<int, Sentence*>* id2SentenceMap = nullptr
    );

    int parseContent(std::string& content, std::span<Sentence*> batchToTransThisRound, const absl::flat_hash_map<int, Sentence*>& id2SentenceMap, const std::string& modelName,
        std::string& backgroudText, TransEngine transEngine, bool showBackgroundText, bool retransAllWhenFail);

    void combineOutputFiles(const fs::path& originalRelFilePath, const absl::flat_hash_map<fs::path, bool>& splitFileParts,
        const fs::path& outputCacheDir, const fs::path& outputDir, std::shared_ptr<spdlog::logger>& logger);

    bool hasRetranslKey(const std::vector<CheckSeCondFunc>& retranslKeys, const json& item, const Sentence* currentSe);

    void saveCache(const std::vector<Sentence>& allSentences, const fs::path& cachePath);

    std::vector<ordered_json> splitJsonArrayNum(const ordered_json& originalData, int chunkSize);
    std::vector<ordered_json> splitJsonArrayEqual(const ordered_json& originalData, int numParts);

    int getSplittedFileIndex(const std::wstring& path);
    int calculateCachePartIndexDiff(const std::wstring& path1, const std::wstring& path2);

    json toml2Json(const toml::value& value);
    ordered_json toml2Json(const toml::ordered_value& tomlData);
}
