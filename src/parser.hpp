#if !defined DACHS_PARSER_HPP_INCLUDED
#define      DACHS_PARSER_HPP_INCLUDED

#include <string>

#include "ast.hpp"

namespace dachs {
namespace syntax {

class parser {
public:
    ast::ast parse(std::string const& code);
};

} // namespace syntax
} // namespace dachs

#endif  // DACHS_PARSER_HPP_INCLUDED
