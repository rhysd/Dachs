#include <cassert>
#include <boost/optional.hpp>
#include "dachs/type.hpp"
#include "dachs/scope.hpp"

namespace dachs {
namespace type {

unknown_type unknown = make<unknown_type>();
weak_unknown_type weak_unknown = unknown;

boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept
{
    static const std::vector<builtin_type> builtin_types
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
