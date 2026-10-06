module;

#include "../GalTranslPP/GPPMacros.hpp"

export module GPPVersion;

export import std;

export NAMESPACE_BEGIN(gpp)

constexpr std::string_view GPPVERSION = "3.2.2";
constexpr std::string_view PYTHONVERSION = "1.0.0";
constexpr std::string_view PROMPTVERSION = "2.0.0";
constexpr std::string_view DICTVERSION = "1.0.3";
constexpr std::string_view QTVERSION = "6.11.1";
constexpr std::string_view ICUVERSION = "7.8.0";

NAMESPACE_END(gpp)
