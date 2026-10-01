#ifndef GPPMACROS
#define GPPMACROS

#ifdef PYBIND11_HEADERS
#include "../3rdParty/3rdModule/pybind11_headers.hpp"
#endif

#ifdef LUABRIDGE3_HEADERS
#include "../3rdParty/3rdModule/luabridge_headers.hpp"
#endif

#define IMPL_LITERAL_TO_STR(x) #x
#define LITERAL_TO_STR(x) IMPL_LITERAL_TO_STR(x)

#endif
