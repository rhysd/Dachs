#if !defined DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED

#include <string>

#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace scope {

std::string stringize_scope_tree(scope_tree const& tree);

} // namespace scope
} // namespace dachs

#endif    // DACHS_HELPER_STRINGIZE_SCOPE_TREE_HPP_INCLUDED

