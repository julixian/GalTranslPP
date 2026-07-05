module;

#include <QCoreApplication>

export module GPPI18n;

export import std.compat;

export
{
	using ::QString;

	QString gppTr(const char* context, const char* source) {
		return QCoreApplication::translate(context, source);
	}
}
