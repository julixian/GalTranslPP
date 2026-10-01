module;

#include "luabridge_headers.hpp"

export module luabridge;

export using ::lua_State;
export using ::lua_Integer;

export namespace luabridge
{
    using ::luabridge::ErrorCode;
    using ::luabridge::LuaException;
    using ::luabridge::LuaRef;
    using ::luabridge::Result;
    using ::luabridge::TypeResult;
    using ::luabridge::Stack;
    using ::luabridge::StackRestore;
    using ::luabridge::enableExceptions;
    using ::luabridge::getGlobal;
    using ::luabridge::getGlobalNamespace;
    using ::luabridge::makeErrorCode;
    using ::luabridge::pairs;
    using ::luabridge::setGlobal;
}

export namespace luabridge::detail
{
    using ::luabridge::detail::is_callable;
}
