module;

#include <toml.hpp>

export module toml11;

export namespace toml
{
    using ::toml::basic_value;
    using ::toml::type_config;
    using ::toml::ordered_type_config;
    using ::toml::value;
    using ::toml::ordered_value;
    using ::toml::value_t;
    using ::toml::array;
    using ::toml::table;
    using ::toml::ordered_array;
    using ::toml::ordered_table;

    using ::toml::preserve_comments;
    using ::toml::discard_comments;
    using ::toml::local_date;
    using ::toml::local_time;
    using ::toml::local_datetime;
    using ::toml::offset_datetime;
    using ::toml::exception;
    using ::toml::syntax_error;
    using ::toml::type_error;
    using ::toml::source_location;
    using ::toml::spec;

    using ::toml::string_format;
    using ::toml::array_format;
    using ::toml::table_format;
    using ::toml::parse;
    using ::toml::parse_str;
    using ::toml::get;
    using ::toml::find;
    using ::toml::find_or;
    using ::toml::find_or_default;
    using ::toml::format;
    using ::toml::to_string;
    using ::toml::visit;
}
