#include <cstdlib>
#include "dachs/compiler.hpp"
#include "dachs/helper/stringize_ast.hpp"

namespace dachs {
std::string compiler::compile(std::string const& code)
{
    auto ast = parser.parse(code);
    return helper::stringize_ast(ast, color_output);
}

}  // namespace dachs
