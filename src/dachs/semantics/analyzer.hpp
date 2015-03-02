#if !defined DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED
#define      DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/semantics_context.hpp"

namespace dachs {
namespace semantics {

semantics_context check_semantics(ast::ast &a, scope::scope_tree &t, syntax::importer &i);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_ANALYZER_HPP_INCLUDED
