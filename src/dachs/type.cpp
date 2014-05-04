#include <cassert>
#include "dachs/type.hpp"

namespace dachs {
namespace type {

builtin_type get_builtin_type(char const* const name) noexcept
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

    assert(false);
}

} // namespace type
} // namespace dachs
