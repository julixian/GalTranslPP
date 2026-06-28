module;

#include "GPPMacros.hpp"
#include <sol/sol.hpp>

export module LuaTextPlugin;

export import GPPDefines;
export import LuaManager;

namespace fs = std::filesystem;

export
{
	class LuaTextPlugin {
	private:
		std::shared_ptr<LuaStateInstance> m_luaState;
		sol::function* m_luaDPreRunFunc = nullptr;
		sol::function* m_luaPreRunFunc = nullptr;
		sol::function* m_luaPostRunFunc = nullptr;
		sol::function* m_luaDPostRunFunc = nullptr;
		sol::function* m_luaUnloadFunc = nullptr;

		std::shared_ptr<spdlog::logger> m_logger;
		std::string m_scriptPath;

	public:

		LuaTextPlugin(const fs::path& projectDir, const std::string& scriptPath, const std::unique_ptr<LuaManager>& luaManager, const std::shared_ptr<spdlog::logger>& logger);
		~LuaTextPlugin();

		void dPreRun(Sentence* se);
		void preRun(Sentence* se);
		void postRun(Sentence* se);
		void dPostRun(Sentence* se);
	};
}
