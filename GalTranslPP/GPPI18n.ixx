module;

#include <QCoreApplication>

export module GPPI18n;

export import std.compat;

export
{
	template<class Arg>
	void applyArg(QString& text, Arg&& arg) {
		text = text.arg(std::forward<Arg>(arg));
	}

	template<class First, class ...Rest>
	void applyArgs(QString& text, First&& first, Rest&&... rest) {
		applyArg(text, std::forward<First>(first));
		if constexpr (sizeof...(rest) > 0) {
			applyArgs(text, std::forward<Rest>(rest)...);
		}
	}

	template<class ...Args>
	std::string gppTr(const char* context, const char* source, Args&&... args) {
		QString text = QCoreApplication::translate(context, source);
		if constexpr (sizeof...(args) > 0) {
			applyArgs(text, std::forward<Args>(args)...);
		}
		return text.toStdString();
	}
}
