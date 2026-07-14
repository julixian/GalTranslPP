module;

#define LUABRIDGE3_HEADERS
#include "GPPMacros.hpp"

export module LuaManager;

export import GPPDefines;
export import SafeQueue;

namespace fs = std::filesystem;

export
{
	namespace luabridge
	{
		template <typename R, typename... Args>
		struct Stack<std::function<R(Args...)>>
		{
			using Type = std::function<R(Args...)>;

			static Result push(lua_State* lua, const Type& function)
			{
				if (!function) {
					lua_pushnil(lua);
					return {};
				}
				Type functionCopy = function;
				LuaRef reference = LuaRef::newFunction(lua, std::move(functionCopy));
				reference.push();
				return {};
			}

			static TypeResult<Type> get(lua_State* lua, int index)
			{
				if (lua_isnil(lua, index)) {
					return Type{};
				}
				if (!lua_isfunction(lua, index)) {
					return makeErrorCode(ErrorCode::InvalidTypeCast);
				}
				LuaRef reference = LuaRef::fromStack(lua, index);
				return Type([reference](Args... args) -> R
					{
						if constexpr (std::is_void_v<R>) {
							lua_State* state = reference.state();
							const StackRestore stackRestore(state);
							reference.push();
							std::error_code pushError;
							const bool argumentsPushed = ([&]()
								{
									const auto pushResult = Stack<std::decay_t<Args>>::push(
										state, std::forward<Args>(args));
									if (!pushResult) {
										pushError = pushResult.error();
										return false;
									}
									return true;
								}() && ...);
							if (!argumentsPushed) {
								throw std::runtime_error(pushError.message());
							}
							if (lua_pcall(state, (int)sizeof...(Args), 0, 0) != LUA_OK) {
								const char* error = lua_tostring(state, -1);
								throw std::runtime_error(error ? error : "未知 Lua 错误");
							}
							return;
						}
						else {
							auto result = reference.call<R>(std::forward<Args>(args)...);
							if (!result) {
								throw std::runtime_error(result.message());
							}
							return std::move(*result);
						}
					});
			}

			static bool isInstance(lua_State* lua, int index)
			{
				return lua_isnil(lua, index) || lua_isfunction(lua, index);
			}
		};
	}



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
				return;
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

