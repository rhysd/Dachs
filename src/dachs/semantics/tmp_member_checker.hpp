#if !defined DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED
#define      DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED

#include <string>

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/probable.hpp"

// Note:
// This class is temporary for array, tuple and range.
// Members should be resolved by the class definitions.

namespace dachs {
namespace semantics {
namespace detail {

using helper::variant::apply_lambda;

struct member_variable_checker : boost::static_visitor<helper::probable<type::type>> {

    std::string const& member_name;
    scope::any_scope const& current_scope;

    member_variable_checker(std::string const& name, scope::any_scope const& c) noexcept
        : member_name(name), current_scope(c)
    {}

    result_type builtin_type(char const* const name) const noexcept
    {
        return type::get_builtin_type(name, type::no_opt);
    }

    result_type operator()(type::tuple_type const& tuple) const
    {
        if (member_name == "size") {
            return builtin_type("uint");
        } else if (member_name == "first") {
            if (tuple->element_types.empty()) {
                return helper::oops("  index out of bounds for tuple '()'");
            }
            return tuple->element_types[0];
        } else if (member_name == "second") {
            if (tuple->element_types.size() < 2) {
                return helper::oops("  index out of bounds for tuple " + tuple->to_string());
            }
            return tuple->element_types[1];
        } else if (member_name == "last") {
            if (tuple->element_types.empty()) {
                return helper::oops("  index out of bounds for tuple '()'");
            }
            return tuple->element_types.back();
        }

        return type::type{};
    }

    result_type operator()(type::array_type const& a) const
    {
        if (member_name == "size") {
            if (!a->size) {
                return helper::oops("  size of array '" + a->to_string() + "' can't be determined");
            }
            return builtin_type("uint");
        }
        return type::type{};
    }

    template<class T>
    result_type operator()(T const&) const
    {
        return type::type{};
    }
};

member_variable_checker::result_type
check_member_var(ast::node::ufcs_invocation const& ufcs, type::type const& child_type, scope::any_scope const& current_scope)
{
    if (ufcs->member_name == "__type") {
        return type::make<type::array_type>(
                type::get_builtin_type("char", type::no_opt),
                child_type.to_string().size()
            );
    }

    member_variable_checker const checker{ufcs->member_name, current_scope};
    return child_type.apply_visitor(checker);
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_TMP_MEMBER_CHECKER_HPP_INCLUDED
