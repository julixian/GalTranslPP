module;

#include "GPPMacros.hpp"

export module DictionaryReviewIndex;

import GPPDefines;

namespace fs = std::filesystem;

export {
    struct DictionaryReviewValueFrequency {
        std::string value;
        int count = 0;
    };

    struct DictionaryReviewSampleSegment {
        int segmentId = -1;
        std::string text;
    };

    struct DictionaryReviewTermGroup {
        std::string sourceTerm;
        std::vector<DictionaryReviewValueFrequency> candidateTargets;
        std::vector<DictionaryReviewValueFrequency> candidateNotes;
        int occurrenceCount = 0;
        std::vector<int> segmentIds;
        std::vector<DictionaryReviewSampleSegment> sampleSegments;
        bool isNameHint = false;
        bool isTokenizerWord = false;
    };

    class DictionaryReviewIndex {
    public:
        static std::vector<DictionaryReviewTermGroup> build(
            const DictList& coarseCandidates,
            const absl::flat_hash_map<std::string, int>& finalCounter,
            const std::vector<std::string>& segments,
            const std::vector<int>& selectedIndices,
            const absl::flat_hash_set<std::string>& nameSet,
            const absl::flat_hash_map<std::string, int>& wordCounter
        );
    };
}
