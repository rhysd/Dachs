#if !defined DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED
#define      DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED

#include <string>

#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace semantics {
namespace detail {

template<class Emitter>
class ctor_checker : public boost::static_visitor<boost::optional<std::string>> {

    ast::node::object_construct const& obj;
    Emitter &emitter;

public:

    ctor_checker(decltype(obj) const& o, Emitter &e)
        : obj(o), emitter(e)
    {}

    result_type operator()(type::array_type const& a)
    {
        if (obj->args.size() > 2) {
            return (boost::format("  Invalid argument for constructor of '%1%' (%2% for 0..2)") % a->to_string() % obj->args.size()).str();
        }

        // XXX:
        // When default constructed, static_array is constructed with null
        if (obj->args.empty() && !a->element_type.is_template()) {
            return boost::none;
        }

        auto const maybe_lit = helper::variant::get_as<ast::node::primary_literal>(obj->args[0]);

        auto const err_msg = (boost::format("  1st argument of constructor of '%1%' must be constant uint") % a->to_string()).str();
        if (!maybe_lit) {
            return err_msg;
        }

        auto const maybe_uint = helper::variant::get_as<unsigned int>((*maybe_lit)->value);
        if (!maybe_uint) {
            return err_msg;
        }

        if (a->size && *a->size <= *maybe_uint) {
            return (boost::format("  Size is out of bounds of staitc_array (size:%1% , specified:%2%)") % *a->size % *maybe_uint).str();
        }

        a->size = *maybe_uint;

        if (obj->args.size() == 1) {
            if (a->element_type.is_template()) {
                return std::string{"  Type of element of array can't be determined"};
            }
            if (!a->element_type.is_default_constructible()) {
                return (
                        boost::format("  Element of static_array '%1%' is not default constructible") % a->to_string()
                    ).str();
            }
            if (a->element_type.is_template()) {
                return std::string{"  Element type of static_array 'can't be determined"};
            }
        } else if (obj->args.size() == 2) {
            auto const elem_type = type::type_of(obj->args[1]);
            if (a->element_type.is_template()) {
                a->element_type = elem_type;
            } else if (elem_type != a->element_type) {
                return (
                    boost::format("  Type of 2nd argument '%1%' doesn't match for constructor of '%2%'\n  Note: '%3%' is expected")
                        % elem_type.to_string()
                        % a->to_string()
                        % a->element_type.to_string()
                ).str();
            }

            if (!emitter.resolve_deep_copy(elem_type, obj)) {
                return "  Invalid copier for '" + elem_type.to_string() + "'";
            }
        }

        return boost::none;
    }

    result_type operator()(type::pointer_type const& p)
    {
        if (obj->args.size() != 1) {
            return (boost::format("  Invalid argument for constructor of '%1%' (%2% for 1)") % p->to_string() % obj->args.size()).str();
        }

        auto const arg_type = type::type_of(obj->args[0]);
        if (!arg_type || p->pointee_type.is_template()) {
            return std::string{"  Invalid pointee element type for pointer construction"};
        }

        if (!arg_type.is_builtin("uint")) {
            return (
                boost::format(
                    "  Type mismatch for the argument of constructor of type '%1%'. '%2%' for 'uint'"
                ) % p->to_string() % arg_type.to_string()
            ).str();
        }

        return boost::none;
    }

    template<class T>
    result_type operator()(T const& t)
    {
        return (boost::format("  Invalid constructor for '%1%'") % t->to_string()).str();
    }
};


} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_TMP_CONSTRUCTOR_CHECKER_HPP_INCLUDED
