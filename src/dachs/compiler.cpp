#include <cstdlib>
#include "dachs/compiler.hpp"
#include "dachs/semantics_check.hpp"
#include "dachs/helper/stringize_ast.hpp"
#include "dachs/helper/stringize_scope_tree.hpp"

namespace dachs {
void compiler::compile(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = scope::make_scope_tree(ast);
    (void) scope_tree;
}

std::string compiler::dump_ast(std::string const& code, bool const colorful)
{
    auto ast = parser.parse(code);
    auto scope_tree = scope::make_scope_tree(ast);
    semantics::check_semantics(ast);
    return helper::stringize_ast(ast, colorful)
         + "\n\n=========Scope Tree========\n\n"
         + helper::stringize_scope_tree(scope_tree);
}

std::string compiler::dump_scopes(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = scope::make_scope_tree(ast);
    return helper::stringize_scope_tree(scope_tree);
}

}  // namespace dachs
