#if !defined DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED
#define      DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED

#include <cstddef>

#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace semantics {

template<class Node, class Scope>
std::size_t dispatch_forward_analyzer(Node &node, Scope const& scope_root);

template<class Scope>
std::size_t check_functions_duplication(Scope const& scope_root);

template<class Node, class Scope>
Scope analyze_ast_node_forward(Node &node, Scope const& scope_root);

scope::scope_tree analyze_symbols_forward(ast::ast &a);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_FORWARD_ANALYZER_HPP_INCLUDED
