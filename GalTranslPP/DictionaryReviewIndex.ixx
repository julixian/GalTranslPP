module;

#include "GPPMacros.hpp"

export module DictionaryReviewIndex;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
    struct DictionaryReviewValueFrequency {
        std::string value;
        int count = 0;
    };

    struct DictionaryReviewTermGroup {
        std::string sourceTerm;
        std::vector<DictionaryReviewValueFrequency> candidateTargets;
        std::vector<DictionaryReviewValueFrequency> candidateNotes;
        std::vector<std::string> sampleSegments;
        int occurrenceCount = 0;
        bool isNameHint = false;
        bool isTokenizerWord = false;
    };

    std::vector<DictionaryReviewTermGroup> buildDictionaryReviewTermGroups(
        const DictList& coarseCandidates,
        const absl::flat_hash_map<std::string, int>& finalCounter,
        const std::vector<std::string>& segments,
        const std::vector<int>& selectedIndices,
        const absl::flat_hash_set<std::string>& nameSet,
        const absl::flat_hash_map<std::string, int>& wordCounter
    );
}
