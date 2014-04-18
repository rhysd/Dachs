#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>

#include "dachs/parser.hpp"

namespace dachs {

class compiler final {
    syntax::parser parser;
public:

    void /*TEMPORARY*/ compile(std::string const& code);

    std::string print_ast(std::string const& code, bool const colorful = true);
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
