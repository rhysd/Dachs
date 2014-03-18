#include "compiler.hpp"

namespace dachs {
std::string compiler::compile(std::string const& code)
{
    auto ast = parser.parse(code);
    (void) ast;
    return "";
}

}  // namespace dachs
