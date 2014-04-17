#if !defined DACHS_PARSER_HPP_INCLUDED
#define      DACHS_PARSER_HPP_INCLUDED

#include <cstddef>
#include <string>
#include <utility>

#include <boost/format.hpp>

#include "dachs/ast.hpp"

namespace dachs {
namespace syntax {

class parser final {
public:
    ast::ast parse(std::string const& code);
};

struct parse_error final : public std::runtime_error {
    std::size_t line, col;

    parse_error(std::size_t const line, std::size_t const col)
        : std::runtime_error((boost::format("Parse error generated at line:%1%, col:%2%") % line % col).str()), line(line), col(col)
    {}

    explicit parse_error(std::pair<std::size_t, std::size_t> const& pos)
        : std::runtime_error((boost::format("Parse error generated at line:%1%, col:%2%") % pos.first % pos.second).str()), line(pos.first), col(pos.second)
    {}
};

} // namespace syntax
} // namespace dachs

#endif  // DACHS_PARSER_HPP_INCLUDED
