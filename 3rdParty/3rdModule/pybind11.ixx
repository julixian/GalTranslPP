module;

#include "pybind11_headers.hpp"

export module pybind11;

export namespace pybind11
{
    using ::pybind11::arg;
    using ::pybind11::bool_;
    using ::pybind11::bytes;
    using ::pybind11::call_guard;
    using ::pybind11::cast;
    using ::pybind11::class_;
    using ::pybind11::dict;
    using ::pybind11::enum_;
    using ::pybind11::error_already_set;
    using ::pybind11::finalize_interpreter;
    using ::pybind11::float_;
    using ::pybind11::function;
    using ::pybind11::gil_scoped_acquire;
    using ::pybind11::gil_scoped_release;
    using ::pybind11::handle;
    using ::pybind11::hasattr;
    using ::pybind11::init;
    using ::pybind11::initialize_interpreter;
    using ::pybind11::int_;
    using ::pybind11::isinstance;
    using ::pybind11::list;
    using ::pybind11::module_;
    using ::pybind11::multiple_interpreters;
    using ::pybind11::none;
    using ::pybind11::object;
    using ::pybind11::reinterpret_borrow;
    using ::pybind11::reinterpret_steal;
    using ::pybind11::repr;
    using ::pybind11::return_value_policy;
    using ::pybind11::str;
    using ::pybind11::subinterpreter;
    using ::pybind11::subinterpreter_scoped_activate;
    using ::pybind11::tuple;
}

export namespace pybind11::detail
{
    using ::pybind11::detail::type_caster;
    using ::pybind11::detail::map_caster;
    using ::pybind11::detail::set_caster;
    using ::pybind11::detail::cast_op;
}
