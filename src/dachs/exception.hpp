#if !defined DACHS_EXCEPTION_HPP_INCLUDED
#define      DACHS_EXCEPTION_HPP_INCLUDED

#include <exception>
#include <cstddef>
#include <boost/format.hpp>

namespace dachs {

class not_implemented_error final : public std::runtime_error {
public:
    not_implemented_error(char const* const file,
                          char const* const func,
                          std::size_t const line,
                          char const* const what_feature) noexcept
        : std::runtime_error((boost::format("In %1%, %2% (line:%3%)\n %4% is not implemented yet.") % file % func % line % what_feature).str())
    {}
};

struct parse_error final : public std::runtime_error {
    std::size_t line, col;

    parse_error(std::size_t const line, std::size_t const col) noexcept
        : std::runtime_error((boost::format("Parse error generated at line:%1%, col:%2%") % line % col).str()), line(line), col(col)
    {}

    explicit parse_error(std::pair<std::size_t, std::size_t> const& pos) noexcept
        : std::runtime_error((boost::format("Parse error generated at line:%1%, col:%2%") % pos.first % pos.second).str()), line(pos.first), col(pos.second)
    {}
};

struct semantic_check_error final : public std::runtime_error {
public:
    semantic_check_error(std::size_t const num, char const* const stage) noexcept
        : std::runtime_error((boost::format("%1% sematic error(s) generated in %2%") % num % stage).str())
    {}
};

} // namespace dachs

#endif    // DACHS_EXCEPTION_HPP_INCLUDED
