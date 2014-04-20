#include <cstdlib>
#include "dachs/compiler.hpp"
#include "dachs/helper/stringize_ast.hpp"

namespace dachs {
void compiler::compile(std::string const& code)
{
    auto ast = parser.parse(code);
    (void) ast;
}

std::string compiler::dump_ast(std::string const& code, bool const colorful)
{
    auto ast = parser.parse(code);
    return helper::stringize_ast(ast, colorful);
}

}  // namespace dachs
