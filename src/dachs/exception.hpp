#if !defined DACHS_EXCEPTION_HPP_INCLUDED
#define      DACHS_EXCEPTION_HPP_INCLUDED

#include <exception>
#include <cstddef>
#include <boost/format.hpp>

#include "dachs/helper/colorizer.hpp"

namespace dachs {

class not_implemented_error final : public std::runtime_error {
public:
    template<class Node, class String>
    not_implemented_error(Node const& node,
                          char const* const file,
                          char const* const func,
                          std::size_t const line,
                          String const& what_feature,
                          helper::colorizer const c = helper::colorizer{}) noexcept
        : std::runtime_error(
                (
                    boost::format(
                            c.red("Error") + " in line:%1%, col:%2%\n" +
                            c.bold("  %3% is not implemented yet.\n") +
                            "  Note: You can contribute to Dachs with implementing this feature. "
                            "Clone https://github.com/rhysd/Dachs and see %4%, %5%(), line:%6%"
                        ) % node->line
                          % node->col
                          % what_feature
                          % file
                          % func
                          % line
                ).str()
            )
    {}

    template<class String>
    not_implemented_error(char const* const file,
                          char const* const func,
                          std::size_t const line,
                          String const& what_feature,
                          helper::colorizer const c = helper::colorizer{}) noexcept
        : std::runtime_error(
                (
                    boost::format(
                            c.red("Error") + '\n' +
                            c.bold("  %1% is not implemented yet.\n") +
                            "  Note: You can contribute to Dachs with implementing this feature. "
                            "Clone https://github.com/rhysd/Dachs and see %2%, %3%(), line:%4%"
                        ) % what_feature
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
        : std::runtime_error((boost::format("%1% semantic error(s) generated in %2%") % num % stage).str())
    {}
};

struct code_generation_error final : public std::runtime_error {
    template<class String>
    code_generation_error(
            std::string const& generator_name,
            String const& msg,
            helper::colorizer const c = helper::colorizer{}
        ) noexcept
        : std::runtime_error(
                c.red("Error\n")
                + c.bold(msg) + "\n"
                "  1 error generated in " + generator_name
            )
    {}

    code_generation_error(std::string const& generator_name, boost::format const& format) noexcept
        : code_generation_error(generator_name, format.str())
    {}
};

} // namespace dachs

#endif    // DACHS_EXCEPTION_HPP_INCLUDED
