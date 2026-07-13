module;

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

	struct LuaStateDeleter {
		void operator()(lua_State* state) const
		{
			if (state) {
				lua_close(state);
			}
		}
	};

	class LuaFunction {
	public:
		explicit LuaFunction(luabridge::LuaRef function) : m_function(std::move(function)) {}

		bool valid() const { return m_function.isCallable(); }

		template<typename Result = void, typename... Args>
		Result call(Args&&... args) const
		{
			if constexpr (std::is_void_v<Result>) {
				lua_State* lua = m_function.state();
				const luabridge::StackRestore stackRestore(lua);
				m_function.push();
				std::error_code pushError;
				const bool argumentsPushed = ([&]()
					{
						const auto pushResult = luabridge::Stack<std::decay_t<Args>>::push(
							lua, std::forward<Args>(args));
						if (!pushResult) {
							pushError = pushResult.error();
							return false;
						}
						return true;
					}() && ...);
				if (!argumentsPushed) {
					throw std::runtime_error(pushError.message());
				}
				if (lua_pcall(lua, (int)sizeof...(Args), 0, 0) != LUA_OK) {
					const char* error = lua_tostring(lua, -1);
					throw std::runtime_error(error ? error : "未知 Lua 错误");
				}
			}
			else {
				auto result = m_function.call<Result>(std::forward<Args>(args)...);
				if (!result) {
					throw std::runtime_error(result.message());
				}
				return std::move(*result);
			}
		}

	private:
		luabridge::LuaRef m_function;
	};

	class LuaStateInstance {
	public:
		std::unique_ptr<lua_State, LuaStateDeleter> m_lua;
		absl::btree_map<std::string, std::unique_ptr<LuaFunction>> m_functions;
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

