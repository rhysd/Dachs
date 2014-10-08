#if !defined DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
#define      DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED

#include <cstddef>
#include <unordered_map>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace semantics {

using captured_offset_map = std::unordered_map<symbol::var_symbol, std::size_t>;
using lambda_captures_type = std::unordered_map<ast::node::function_definition, captured_offset_map>;

struct semantics_context {
    scope::scope_tree scopes;
    lambda_captures_type lambda_captures;

    semantics_context(semantics_context const&) = delete;
    semantics_context &operator=(semantics_context const&) = delete;
    semantics_context(semantics_context &&) = default;
    semantics_context &operator=(semantics_context &&) = default;
};

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
