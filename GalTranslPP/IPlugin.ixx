module;

#include <spdlog/spdlog.h>

export module IPlugin;

export import Tool;

namespace fs = std::filesystem;

export {
	class IPlugin {

	protected:

		fs::path m_projectDir;
		std::shared_ptr<spdlog::logger> m_logger;

	public:

		virtual void run(Sentence* se) = 0;

		IPlugin(const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger);

		virtual ~IPlugin();
	};

	std::vector<std::shared_ptr<IPlugin>> registerPlugins(const std::vector<std::string>& pluginNames, const fs::path& projectDir, std::shared_ptr<spdlog::logger> logger);
}
