module;

#define PYBIND11_HEADERS
#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module ProblemAnalyzer;

export import Dictionary;

export
{
    struct ProblemCompareObj {
        bool use = false;
        CachePart base = CachePart::Orig;
        CachePart check = CachePart::Transview;
    };

	struct Problems {
        ProblemCompareObj highFrequency;
        ProblemCompareObj punctsMiss;
        ProblemCompareObj remainJp;
        ProblemCompareObj introLatin;
        ProblemCompareObj introHangul;
        ProblemCompareObj introTraditionalChinese;
        ProblemCompareObj linebreakLost;
        ProblemCompareObj linebreakAdded;
        ProblemCompareObj longer;
        ProblemCompareObj strictlyLonger;
        ProblemCompareObj dictUnused;
        ProblemCompareObj notTargetLang;
        ProblemCompareObj invalidChar;
	};

	class ProblemAnalyzer {

	private:

        const std::unique_ptr<GptDictionary>& m_gptDictionary;

        Problems m_problems;
        std::vector<std::string> m_punctsToCheck;
        double m_probabilityThreshold{};
        std::string m_codePage;
        std::string m_targetLang;

        std::shared_ptr<spdlog::logger> m_logger;

	public:

        explicit ProblemAnalyzer(const std::unique_ptr<GptDictionary>& gptDictionary, const std::string& targetLang,
            const std::string& punctSet, const std::string& codePage, double langProbability,
            const std::shared_ptr<spdlog::logger>& logger);

        ~ProblemAnalyzer();

		void setProblemRule(const std::string& problemKey, bool enabled, const std::string& base, const std::string& check);

		void analyze(Sentence* sentence);
	};
}
