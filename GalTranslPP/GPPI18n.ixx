module;

#include <QCoreApplication>

export module GPPI18n;

export import std.compat;

export
{
	template<class ...Args>
	std::string gppTr(const char* context, const char* source, Args&&... args) {
		if constexpr (sizeof...(args) > 0) {
			return QCoreApplication::translate(context, source).arg(std::forward<Args>(args)...).toStdString();
		}
		else {
			return QCoreApplication::translate(context, source).toStdString();
		}
	}
}
