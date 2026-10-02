module;

#include "GPPMacros.hpp"

#include <QCoreApplication>

export module GPPI18n;

export import std;

export NAMESPACE_BEGIN(gpp)

using ::QString;

QString gppTr(const char* context, const char* source) {
	return QCoreApplication::translate(context, source);
}

NAMESPACE_END(gpp)
