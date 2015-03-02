#if !defined DACHS_PARSER_PARSER_HPP_INCLUDED
#define      DACHS_PARSER_PARSER_HPP_INCLUDED

#include <string>
#include <vector>
#include <utility>
#include <cstddef>

#include <boost/format.hpp>

#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace syntax {

class parser final {
public:
    ast::ast parse(
            std::string const& code,
            std::string const& file_name
        ) const;

    void check_syntax(
            std::string const& code
        ) const;
};

} // namespace syntax
} // namespace dachs

#endif  // DACHS_PARSER_PARSER_HPP_INCLUDED
