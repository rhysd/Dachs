#if !defined DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
#define      DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED

#include <unordered_map>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace semantics {

using capture_map_type = std::unordered_map<symbol::var_symbol, symbol::var_symbol>;
using lambda_captures_type = std::unordered_map<ast::node::function_definition, capture_map_type>;

struct semantics_context {
    scope::scope_tree scopes;
    lambda_captures_type lambda_captures;
};

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
