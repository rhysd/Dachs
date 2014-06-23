#if !defined DACHS_EXCEPTION_HPP_INCLUDED
#define      DACHS_EXCEPTION_HPP_INCLUDED

#include <exception>
#include <cstddef>
#include <boost/format.hpp>

namespace dachs {

class not_implemented_error final : public std::runtime_error {
public:
    template<class Node>
    not_implemented_error(Node const& node,
                          char const* const file,
                          char const* const func,
                          std::size_t const line,
                          char const* const what_feature) noexcept
        : std::runtime_error(
                (
                    boost::format(
                            "At line:%1%, col:%2%\n %3% is not implemented yet.\n"
                            "Note: You can contribute to Dachs with implementing this feature. "
                            "See file:%4%, %5%(), line:%6%, "
                            "and clone https://github.com/rhysd/Dachs.")
                        % node->line
                        % node->col
                        % what_feature
                        % file
                        % func
                        % line
                ).str()
            )
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
    semantic_check_error(std::size_t const num, char const* const stage) noexcept
        : std::runtime_error((boost::format("%1% sematic error(s) generated in %2%") % num % stage).str())
    {}
};

struct code_generation_error final : public std::runtime_error {
    code_generation_error(std::string const& generator_name, std::string const& msg) noexcept
        : std::runtime_error("Code generation with " + generator_name + " failed: " + msg)
    {}
};

} // namespace dachs

#endif    // DACHS_EXCEPTION_HPP_INCLUDED
