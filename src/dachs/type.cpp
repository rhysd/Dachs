#include <cassert>
#include <boost/optional.hpp>
#include "dachs/type.hpp"
#include "dachs/scope.hpp"

namespace dachs {
namespace type {

boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept
{
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

    for (auto const& t : builtin_types) {
        if (t->name == name) {
            return t;
        }
    }

    return boost::none;
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
    return "<funcref" + (ref ? ':' + ref->lock()->name + '>' : ">");
}

} // namespace type_node
} // namespace dachs
