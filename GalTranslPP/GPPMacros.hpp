#ifndef GPPMACROS
#define GPPMACROS

#define _RANGES_ // import std.compat; 时如果有头文件 #include <ranges> 会导致调用 std::views::zip 时出现异常定义不一致的问题。这是 STL 的和 MSVC 的问题，不知道后续能不能修复

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

#ifdef ABSL_CONTAINERS
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/btree_map.h>
#include <absl/container/btree_set.h>
#endif // 用模块也能编过但是会崩。。。不知道又是哪里的什么在抽风。。。

#define NESTED_CVT(className, memberName) sol::property([](className& self) \
{ \
	return sol::nested<decltype(className::memberName)>(self.memberName); \
}, [](className& self, decltype(className::memberName) table) { self.memberName = std::move(table); }) 

#endif
