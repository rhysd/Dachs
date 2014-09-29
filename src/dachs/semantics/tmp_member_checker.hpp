#if !defined DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED
#define      DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED

#include <string>

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"

// Note:
// This class is temporary for array, tuple and range.
// Members should be resolved by the class definitions.

namespace dachs {
namespace semantics {
namespace detail {

struct member_variable_checker : boost::static_visitor<boost::variant<type::type, std::string>> {

    std::string const& member_name;

    member_variable_checker(std::string const& name) noexcept
        : member_name(name)
    {}

    result_type builtin_type(char const* const name) const noexcept
    {
        return type::get_builtin_type(name, type::no_opt);
    }

    result_type operator()(type::tuple_type const& tuple) const
    {
        if (member_name == "size") {
            return builtin_type("uint");
        } else if (member_name == "first" || member_name == "last") {
            if (tuple->element_types.empty()) {
                return "index out of bounds for tuple '()'";
            }
            return tuple->element_types[0];
        } else if (member_name == "second") {
            if (tuple->element_types.size() < 2) {
                return "index out of bounds for tuple " + tuple->to_string();
            }
            return tuple->element_types[1];
        }

        return '\'' + member_name + "' is not a member of tuple";
    }

    result_type operator()(type::array_type const& ) const
    {
        if (member_name == "size") {
            return builtin_type("uint");
        }
        return '\'' + member_name + "' is not a member of array";
    }

    template<class T>
    result_type operator()(T const& t) const
    {
        return '\'' + member_name + "' is not a member of " + t->to_string();
    }
};

member_variable_checker::result_type
check_member_var(ast::node::ufcs_invocation const& ufcs)
{
    if (ufcs->member_name == "__type") {
        return type::get_builtin_type("string", type::no_opt);
    }
    return type::type_of(ufcs->child).apply_visitor(member_variable_checker{ufcs->member_name});
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED
