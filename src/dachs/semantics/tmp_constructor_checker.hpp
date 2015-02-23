#if !defined DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED
#define      DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED

#include <string>

#include <boost/optional.hpp>
#include <boost/format.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace semantics {
namespace detail {

struct ctor_checker {

    using result_type = boost::optional<std::string>;

    template<class Exprs>
    result_type operator()(type::type const& type, Exprs const& args) const
    {
        return type.apply_lambda([this, &args](auto const& t){ return (*this)(t, args); });
    }

    template<class Exprs>
    result_type operator()(type::array_type const& a, Exprs const& args) const
    {
        if (args.size() != 1 && args.size() != 2) {
            return (boost::format("  Invalid argument for constructor of '%1%' (%2% for 1 or 2)") % a->to_string() % args.size()).str();
        }

        auto const maybe_lit = helper::variant::get_as<ast::node::primary_literal>(args[0]);

        auto const err_msg = (boost::format("  1st argument of constructor of '%1%' must be constant uint") % a->to_string()).str();
        if (!maybe_lit) {
            return err_msg;
        }

        auto const maybe_uint = helper::variant::get_as<unsigned int>((*maybe_lit)->value);
        if (!maybe_uint) {
            return err_msg;
        }

        if (a->size && *a->size <= *maybe_uint) {
            return (boost::format("  Size is out of bounds of the array type (size:%1% , specified:%2%)") % *a->size % *maybe_uint).str();
        }

        a->size = *maybe_uint;

        if (args.size() == 1) {
            if (!a->element_type.is_default_constructible()) {
                return (
                        boost::format("  Element of array '%1%' is not default constructible") % a->to_string()
                    ).str();
            }
        } else if (args.size() == 2) {
            auto const elem_type = type::type_of(args[1]);
            if (elem_type != a->element_type) {
                return (
                    boost::format("  Type of 2nd argument '%1%' doesn't match for constructor of '%2%'\n  Note: '%3%' is expected")
                        % elem_type.to_string()
                        % a->to_string()
                        % a->element_type.to_string()
                ).str();
            }
        }
        return boost::none;
    }

    template<class T, class Exprs>
    result_type operator()(T const& t, Exprs const&) const
    {
        return (boost::format("  Invalid constructor for '%1%'") % t->to_string()).str();
    }
};


} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED
