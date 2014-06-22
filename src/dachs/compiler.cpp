#include <cstdlib>

#include "dachs/compiler.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/semantics/stringize_scope_tree.hpp"
#include "dachs/codegen/llvmir/code_generator.hpp"

namespace dachs {
void compiler::compile(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    (void) scope_tree;
}

std::string compiler::dump_ast(std::string const& code, bool const colorful)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    return ast::stringize_ast(ast, colorful)
         + "\n\n=========Scope Tree========\n\n"
         + scope::stringize_scope_tree(scope_tree);
}

std::string compiler::dump_scopes(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    return scope::stringize_scope_tree(scope_tree);
}

}  // namespace dachs
