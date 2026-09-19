module;

#define JPCRE2_BUILD_MODULE
#include "jpcre2.hpp"

export module jpcre2;

export namespace jpcre2
{
	using ::jpcre2::SIZE_T;
	using ::jpcre2::Uint;
	using ::jpcre2::Ush;
	using ::jpcre2::VecOff;
	using ::jpcre2::VecOpt;

	using ::jpcre2::ModifierTable;
	using ::jpcre2::select;

	using ::jpcre2::NONE;
	using ::jpcre2::FIND_ALL;
	using ::jpcre2::JIT_COMPILE;
}

export
{
	using jpc = jpcre2::select<char>;
}
