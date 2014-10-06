#include <cstddef>
#include <iostream>

#include <boost/format.hpp>

#include "dachs/exception.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/forward_analyzer.hpp"
#include "dachs/semantics/analyzer.hpp"

namespace dachs {
namespace semantics {

semantics_context analyze_semantics(ast::ast &a)
{
    auto tree = analyze_symbols_forward(a);
    auto captures = check_semantics(a, tree);

    // TODO: Get type of global function variables' type on visit node::function_definition
    // Note:
    // Type of function can be set after parameters' types are set.
    // Should function type be set at forward analysis phase?
    // If so, type calculation pass should be separated from symbol analysis pass.

    return {tree, std::move(captures)};
}

} // namespace semantics
} // namespace dachs
