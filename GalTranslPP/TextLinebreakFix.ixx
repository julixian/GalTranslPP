module;

#include "GPPMacros.hpp"
#include <toml.hpp>

export module TextLinebreakFix;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
	enum class LinebreakFixMode
	{
		None, Average, FixCharCount, KeepPositions, PreferPunctuations, CheckOnly
	};

	class TextLinebreakFix {

	private:

		std::function<NLPResult(const std::string&)> m_tokenizeTargetLangFunc;
		fs::path m_tokenizeCachePath;
		std::shared_mutex m_tokenizeCacheMapMutex;
		absl::flat_hash_map<std::string, WordPosVec> m_tokenizeCacheMap;

		std::shared_ptr<spdlog::logger> m_logger;

		LinebreakFixMode m_mode;
		PluginRunTime m_runTime;
		double m_priorityThreshold;
		int m_segmentThreshold;
		int m_errorThreshold;
		bool m_forceFix;
		bool m_useTokenizer;

		std::vector<std::string_view> splitIntoTokenViews(std::string_view str);

	public:

		TextLinebreakFix(const fs::path& otherCacheDir, const toml::value& projectConfig, const std::shared_ptr<spdlog::logger>& logger, PluginRunTime runTime);
		~TextLinebreakFix();

		void dPostRun(Sentence* se);
	};
}
