module;

#include "GPPMacros.hpp"

export module ProblemAnalyzer;

export import Dictionary;

export
{
    struct ProblemCompareObj {
        bool use = false;
        CachePart base = CachePart::OrigText;
        CachePart check = CachePart::TransPreview;
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

        explicit ProblemAnalyzer(const std::unique_ptr<GptDictionary>& gptDictionary, const std::string& targetLang, const std::shared_ptr<spdlog::logger>& logger);

        ~ProblemAnalyzer();

		void loadProblems(const std::vector<std::string>& problemList, const std::string& punctSet, const std::string& codePage, double langProbability);

        void overwriteCompareObj(const std::string& problemKey, const std::string& base, const std::string& check);

		void analyze(Sentence* sentence);
	};
}
