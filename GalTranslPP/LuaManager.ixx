module;

#define SOL2_HEADERS
#include "GPPMacros.hpp"

export module LuaManager;

export import GPPDefines;
export import SafeQueue;

namespace fs = std::filesystem;

export
{
	struct LuaTask {
		std::function<void()> taskFunc;
		std::promise<void> promise;
	};

	class LuaStateInstance {
	public:
		std::unique_ptr<sol::state> m_lua;
		absl::btree_map<std::string, std::unique_ptr<sol::function>> m_functions;
		LuaStateInstance();
		~LuaStateInstance();

		std::future<void> submitTask(std::function<void()> taskFunc);

	private:
		void daemonThreadFunc();

		SafeQueue<std::unique_ptr<LuaTask>> m_taskQueue;
		std::thread m_daemonThread;
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

