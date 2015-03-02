#if !defined DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED
#define      DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/semantics_context.hpp"

namespace dachs {
namespace semantics {

// FIXME: argument should be const
semantics_context analyze_semantics(ast::ast &a, syntax::importer &i);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTIC_ANALYSIS_HPP_INCLUDED
