module;

#include "GPPMacros.hpp"

module DictionaryReviewIndex;

struct ReviewBucket {
    std::vector<std::string> targetOrder;
    absl::flat_hash_map<std::string, int> targetCounts;
    std::vector<std::string> noteOrder;
    absl::flat_hash_map<std::string, int> noteCounts;
};

std::vector<DictionaryReviewValueFrequency> buildValueFrequencies(
    const std::vector<std::string>& order,
    const absl::flat_hash_map<std::string, int>& counts
) {
    std::vector<DictionaryReviewValueFrequency> values;
    values.reserve(order.size());
    for (const std::string& value : order) {
        const auto it = counts.find(value);
        if (it == counts.end()) {
            continue;
        }
        values.push_back({
            .value = value,
            .count = it->second
        });
    }

    std::ranges::sort(values, [](const DictionaryReviewValueFrequency& a, const DictionaryReviewValueFrequency& b)
        {
            if (a.count != b.count) {
                return a.count > b.count;
            }
            return a.value < b.value;
        });
    return values;
}

std::vector<DictionaryReviewTermGroup> DictionaryReviewIndex::build(
    const DictList& coarseCandidates,
    const absl::flat_hash_map<std::string, int>& finalCounter,
    const std::vector<std::string>& segments,
    const std::vector<int>& selectedIndices,
    const absl::flat_hash_set<std::string>& nameSet,
    const absl::flat_hash_map<std::string, int>& wordCounter
) {
    absl::btree_map<std::string, ReviewBucket> grouped;
    for (const auto& [sourceTerm, targetTerm, note] : coarseCandidates) {
        if (sourceTerm.empty()) {
            continue;
        }

        ReviewBucket& bucket = grouped[sourceTerm];
        if (!targetTerm.empty()) {
            if (!bucket.targetCounts.contains(targetTerm)) {
                bucket.targetOrder.push_back(targetTerm);
            }
            ++bucket.targetCounts[targetTerm];
        }
        if (!note.empty()) {
            if (!bucket.noteCounts.contains(note)) {
                bucket.noteOrder.push_back(note);
            }
            ++bucket.noteCounts[note];
        }
    }

    std::vector<DictionaryReviewTermGroup> groups;
    groups.reserve(grouped.size());
    for (const auto& [sourceTerm, bucket] : grouped) {
        DictionaryReviewTermGroup group;
        group.sourceTerm = sourceTerm;
        group.candidateTargets = buildValueFrequencies(bucket.targetOrder, bucket.targetCounts);
        group.candidateNotes = buildValueFrequencies(bucket.noteOrder, bucket.noteCounts);
        if (const auto it = finalCounter.find(sourceTerm); it != finalCounter.end()) {
            group.occurrenceCount = it->second;
        }
        else {
            group.occurrenceCount = std::max(1, (int)group.candidateTargets.size());
        }
        group.isNameHint = nameSet.contains(sourceTerm);
        group.isTokenizerWord = wordCounter.contains(sourceTerm);

        for (int segmentId : selectedIndices) {
            if (segmentId < 0 || segmentId >= (int)segments.size()) {
                continue;
            }
            const std::string& segment = segments[segmentId];
            if (!segment.contains(sourceTerm)) {
                continue;
            }
            group.segmentIds.push_back(segmentId);
            if (group.sampleSegments.size() < 3) {
                group.sampleSegments.push_back({
                    .segmentId = segmentId,
                    .text = segment
                });
            }
        }

        groups.push_back(std::move(group));
    }

    std::ranges::sort(groups, [](const DictionaryReviewTermGroup& a, const DictionaryReviewTermGroup& b)
        {
            if (a.occurrenceCount != b.occurrenceCount) {
                return a.occurrenceCount > b.occurrenceCount;
            }
            return a.sourceTerm < b.sourceTerm;
        });
    return groups;
}
