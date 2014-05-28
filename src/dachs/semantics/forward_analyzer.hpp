#if !defined DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED
#define      DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED

#include <cstddef>

#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace semantics {

scope::scope_tree analyze_symbols_forward(ast::ast &a);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED
