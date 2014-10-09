#if !defined DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
#define      DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED

#include <cstddef>
#include <unordered_map>

#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace semantics {

// Note:
// The map owns the ownership of the symbol which is replaced as a aptured symbol.
using captured_offset_map = std::unordered_map<symbol::var_symbol, std::size_t>;
using lambda_captures_type = std::unordered_map<scope::func_scope, captured_offset_map>;

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
