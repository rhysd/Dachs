#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>

#include "dachs/parser.hpp"

namespace dachs {

class compiler final {
    syntax::parser parser;
    bool color_output;
public:

    explicit compiler(bool const color_output = true)
        : color_output(color_output)
    {}

    std::string /*TEMPORARY*/ compile(std::string const& code);
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
