#include <cstdlib>
#include "compiler.hpp"
#include "helper/stringize_ast.hpp"

namespace dachs {
std::string compiler::compile(std::string const& code)
{
    try {
        auto ast = parser.parse(code);
        return helper::stringize_ast(ast);
    } catch (syntax::parse_error const& e) {
        std::cerr << e.what() << std::endl;
        std::exit(4);
    }
}

}  // namespace dachs
