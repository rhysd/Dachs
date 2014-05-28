#if !defined DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED
#define      DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"

namespace dachs {
namespace semantics {

// FIXME: argument should be const
scope::scope_tree analyze_semantics(ast::ast &a);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED
