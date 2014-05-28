#if !defined DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED
#define      DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"

namespace dachs {
namespace semantics {

void check_semantics(ast::ast &a, scope::scope_tree &t);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED
