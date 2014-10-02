#if !defined DACHS_SEMANTICS_FORWARD_FUNCTION_ANALYZER_HPP_INCLUDED
#define      DACHS_SEMANTICS_FORWARD_FUNCTION_ANALYZER_HPP_INCLUDED

#include <cstddef>

#include "dachs/ast/ast_fwd.hpp"

// Note:
// This visitor moves all unnamed functions and member functions to global space.
// It gathers all function definitions to one place.

namespace dachs {
namespace semantics {

// Note:
// Returns number of errors
std::size_t dispatch_forward_function_analyzer(ast::ast const&);

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_FORWARD_FUNCTION_ANALYZER_HPP_INCLUDED
