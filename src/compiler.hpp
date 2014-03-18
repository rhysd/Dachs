#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>

#include "parser.hpp"

namespace dachs {

class compiler {
    syntax::parser parser;
public:
    std::string /*TEMPORARY*/ compile(std::string const& code);
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
