#if !defined DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED

#include <string>

#include "dachs/scope.hpp"

namespace dachs {
namespace helper {

std::string stringize_scope_tree(scope::scope_tree const& tree);

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED

