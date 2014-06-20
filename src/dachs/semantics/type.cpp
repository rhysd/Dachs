#include <cassert>
#include <boost/optional.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace type {
namespace detail {

static std::vector<builtin_type> const builtin_types
    = {
        make<builtin_type>("int"),
        make<builtin_type>("uint"),
        make<builtin_type>("float"),
        make<builtin_type>("char"),
        make<builtin_type>("bool"),
        make<builtin_type>("string"),
        make<builtin_type>("symbol"),
    };

} // namespace detail

no_opt_t no_opt;

boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept
{
    for (auto const& t : detail::builtin_types) {
        if (t->name == name) {
            return t;
        }
    }

    return boost::none;
}

builtin_type get_builtin_type(char const* const name, no_opt_t) noexcept
{
    for (auto const& t : detail::builtin_types) {
        if (t->name == name) {
            return t;
        }
    }

    std::abort();
}

bool any_type::operator==(any_type const& rhs) const noexcept
{
    return apply_lambda(
            [](auto const& l, auto const& r)
            {
                if (!l || !r) {
                    // If both sides are empty, return true.  Otherwise return false.
                    return !l && !r;
                } else {
                    return *l == *r;
                }
            }, value, rhs.value);
}

} // namespace type

namespace type_node {

bool func_ref_type::operator==(func_ref_type const& rhs) const noexcept
{
    if (!ref && !rhs.ref) {
        return true;
    }

    if (ref && rhs.ref) {
        return ref->lock()->name == rhs.ref->lock()->name;
    }

    return false;
}

std::string func_ref_type::to_string() const noexcept
{
    return "<funcref" + (ref ? ':' + ref->lock()->to_string() + '>' : ">");
}

boost::optional<ast::node::parameter> template_type::get_ast_node_as_parameter() const noexcept
{
    return ast::node::get_shared_as<ast::node::parameter>(ast_node);
}

std::string template_type::to_string() const noexcept
{
    if (auto maybe_param = get_ast_node_as_parameter()) {
        return "<template:" + (*maybe_param)->to_string() + ">";
    } else {
        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }
}

} // namespace type_node
} // namespace dachs
