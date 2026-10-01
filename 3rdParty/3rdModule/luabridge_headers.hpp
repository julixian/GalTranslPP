#pragma once

#include <lua.hpp>

// Share the same configuration between the module and files using Lua macros.
#ifndef LUABRIDGE_DISABLE_CXX17_FILESYSTEM
#define LUABRIDGE_DISABLE_CXX17_FILESYSTEM
#endif
#ifndef LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING
#define LUABRIDGE_SAFE_LUA_C_EXCEPTION_HANDLING 1
#endif

#include <luabridge3/LuaBridge/LuaBridge.h>
#include <luabridge3/LuaBridge/Array.h>
#include <luabridge3/LuaBridge/Map.h>
#include <luabridge3/LuaBridge/Set.h>
#include <luabridge3/LuaBridge/UnorderedMap.h>
#include <luabridge3/LuaBridge/UnorderedSet.h>
#include <luabridge3/LuaBridge/Vector.h>
