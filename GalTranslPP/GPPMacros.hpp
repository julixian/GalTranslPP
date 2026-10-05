#pragma once

#ifdef PYBIND11_HEADERS
#include "pybind11_headers.hpp"
#endif

#ifdef LUABRIDGE3_HEADERS
#include "luabridge_headers.hpp"
#endif

#define GPP_EMPTY_DEFINE
#define GPP_EMPTY_FUNCLIKE_DEFINE(...)

#define NAMESPACE_BEGIN(name) namespace name {
#define NAMESPACE_END(name) }

#define IMPL_LITERAL_TO_STR(x) #x
#define LITERAL_TO_STR(x) IMPL_LITERAL_TO_STR(x)
