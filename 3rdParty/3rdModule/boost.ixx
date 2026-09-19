module;

#include <boost/crc.hpp>
#include <boost/algorithm/string.hpp>

export module boost;

export namespace boost
{
	namespace algorithm
	{
		using ::boost::algorithm::istarts_with;
		using ::boost::algorithm::iends_with;
		using ::boost::algorithm::icontains;
		using ::boost::algorithm::iequals;

		using ::boost::algorithm::replace_all;
		using ::boost::algorithm::replace_all_copy;
	}

	using ::boost::crc_optimal;
}
