#if !defined DACHS_PARSER_PARSER_HPP_INCLUDED
#define      DACHS_PARSER_PARSER_HPP_INCLUDED

#include <cstddef>
#include <string>
#include <utility>

#include <boost/format.hpp>

#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace syntax {

class parser final {
public:
    ast::ast parse(std::string const& code, std::string const& file_name) const;
};

} // namespace syntax
} // namespace dachs

#endif  // DACHS_PARSER_PARSER_HPP_INCLUDED
