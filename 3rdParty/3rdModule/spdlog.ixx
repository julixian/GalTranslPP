module;

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

export module spdlog;

export namespace fmt
{
	using ::fmt::to_string;
}

export namespace spdlog 
{
	using ::spdlog::trace;
	using ::spdlog::debug;
	using ::spdlog::info;
	using ::spdlog::warn;
	using ::spdlog::error;
	using ::spdlog::critical;

	using ::spdlog::logger;
	using ::spdlog::sink_ptr;
	using ::spdlog::memory_buf_t;
	using ::spdlog::formatter;

	namespace level
	{
		using level::level_enum;
		using level::trace;
		using level::debug;
		using level::info;
		using level::warn;
		using level::err;
		using level::critical;
	}

	namespace sinks
	{
		using sinks::sink;
		using sinks::base_sink;
		using sinks::rotating_file_sink_mt;
	}

	namespace details
	{
		using details::log_msg;
	}
}
