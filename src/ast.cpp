#include "ast.hpp"

namespace dachs {
namespace syntax {
namespace ast {
namespace node_type {

std::size_t generate_id()
{
    static std::size_t current_id = 0;
    return ++current_id;
}

} // namespace node_type
} // namespace ast
} // namespace syntax
} // namespace dachs
