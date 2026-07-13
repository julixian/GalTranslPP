#ifndef GPPMACROS
#define GPPMACROS

#define _RANGES_ // import std.compat; 时如果有头文件 #include <ranges> 会导致调用 std::views::zip 时出现异常定义不一致的问题。这是 STL 的和 MSVC 的问题，不知道后续能不能修复

#define PROJECT_NO_ANSI

// 已经添加至预处理器定义中
//_CRT_SECURE_NO_WARNINGS
//SPDLOG_WCHAR_FILENAMES

#ifdef PYBIND11_HEADERS 
#define PYBIND11_DETAILED_ERROR_MESSAGES
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/complex.h>
#include <pybind11/functional.h>
#include <pybind11/stl_bind.h>
#include <pybind11/embed.h>
#include <pybind11/subinterpreter.h>
#endif

#include <lua.hpp>
#define LUABRIDGE_DISABLE_CXX17_FILESYSTEM
#define LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING 1
#include <luabridge3/LuaBridge/LuaBridge.h>
#include <luabridge3/LuaBridge/Array.h>
#include <luabridge3/LuaBridge/Map.h>
#include <luabridge3/LuaBridge/Set.h>
#include <luabridge3/LuaBridge/UnorderedMap.h>
#include <luabridge3/LuaBridge/UnorderedSet.h>
#include <luabridge3/LuaBridge/Vector.h>
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

namespace luabridge
{
	template <typename K, typename V, typename Hash, typename Eq, typename Allocator>
	struct Stack<absl::flat_hash_map<K, V, Hash, Eq, Allocator>>
	{
		using Type = absl::flat_hash_map<K, V, Hash, Eq, Allocator>;

		static Result push(lua_State* lua, const Type& value)
		{
			const std::unordered_map<K, V> converted(value.begin(), value.end());
			return Stack<std::unordered_map<K, V>>::push(lua, converted);
		}

		static TypeResult<Type> get(lua_State* lua, int index)
		{
			auto converted = Stack<std::unordered_map<K, V>>::get(lua, index);
			if (!converted) {
				return converted.error();
			}
			return Type(converted->begin(), converted->end());
		}

		static bool isInstance(lua_State* lua, int index)
		{
			return Stack<std::unordered_map<K, V>>::isInstance(lua, index);
		}
	};

	template <typename K, typename Hash, typename Eq, typename Allocator>
	struct Stack<absl::flat_hash_set<K, Hash, Eq, Allocator>>
	{
		using Type = absl::flat_hash_set<K, Hash, Eq, Allocator>;

		static Result push(lua_State* lua, const Type& value)
		{
			const std::unordered_set<K> converted(value.begin(), value.end());
			return Stack<std::unordered_set<K>>::push(lua, converted);
		}

		static TypeResult<Type> get(lua_State* lua, int index)
		{
			auto converted = Stack<std::unordered_set<K>>::get(lua, index);
			if (!converted) {
				return converted.error();
			}
			return Type(converted->begin(), converted->end());
		}

		static bool isInstance(lua_State* lua, int index)
		{
			return Stack<std::unordered_set<K>>::isInstance(lua, index);
		}
	};

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

/* sol2 迁移完成后保留旧宏实现作为参考。
#define SOL_ALL_SAFETIES_ON 1
#include <filesystem>
#include <sol/sol.hpp>
#define NESTED_CVT(className, memberName) sol::property([](className& self) \
{ \
	return sol::nested<decltype(className::memberName)>(self.memberName); \
}, [](className& self, decltype(className::memberName) table) { self.memberName = std::move(table); })
namespace sol
{
	template <>
	struct is_container<std::filesystem::path> : std::false_type {};

	template <>
	struct is_to_stringable<std::filesystem::path> : std::false_type {};

	template <>
	struct is_automagical<std::filesystem::path> : std::false_type {};
}
*/

#define IMPL_LITERAL_TO_STR(x) #x
#define LITERAL_TO_STR(x) IMPL_LITERAL_TO_STR(x)

#endif
