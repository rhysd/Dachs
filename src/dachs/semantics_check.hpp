#if !defined DACHS_SEMANTIC_CHECK_HPP_INCLUDED
#define      DACHS_SEMANTIC_CHECK_HPP_INCLUDED

#include "dachs/ast.hpp"

namespace dachs {
namespace semantics {

// FIXME: argument should be const
bool check_semantics(ast::ast &a);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTIC_CHECK_HPP_INCLUDED
