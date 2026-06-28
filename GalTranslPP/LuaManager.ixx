module;

#include "GPPMacros.hpp"
#include <sol/sol.hpp>

export module LuaManager;

export import GPPDefines;

namespace fs = std::filesystem;

export
{
	struct LuaStateInstance {
		std::unique_ptr<sol::state> lua;
		absl::btree_map<std::string, std::unique_ptr<sol::function>> functions;
		std::mutex executionMutex; // 用于保护这个特定 lua 实例的执行
		LuaStateInstance() : lua(std::make_unique<sol::state>()) {}
	};

	class LuaManager {

	public:

		explicit LuaManager(const std::shared_ptr<spdlog::logger>& logger) : m_logger(logger) {}
		
		std::optional<std::shared_ptr<LuaStateInstance>> registerFunction
		(const std::string& scriptPath, const std::string& functionName);

	private:

		absl::btree_map<fs::path, std::shared_ptr<LuaStateInstance>> m_scriptStates;

		std::shared_ptr<spdlog::logger> m_logger;

		void registerCustomTypes(const std::shared_ptr<LuaStateInstance>& luaStateInstance, const std::string& scriptPath);
	};
}

