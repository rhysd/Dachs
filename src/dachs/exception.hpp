#if !defined DACHS_EXCEPTION_HPP_INCLUDED
#define      DACHS_EXCEPTION_HPP_INCLUDED

#include <exception>
#include <cstddef>
#include <boost/format.hpp>

#include "dachs/helper/colorizer.hpp"
#include "dachs/ast/ast_fwd.hpp"

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
                            c.red("Error") + " in %1%\n" +
                            c.bold("  %2% is not implemented yet.\n") +
                            "  Note: You can contribute to Dachs with implementing this feature. "
                            "Clone https://github.com/rhysd/Dachs and see %3%, %4%(), line:%5%"
                        ) % node->location.to_string()
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
    ast::location_type location;

    explicit parse_error(ast::location_type const& loc)
        : std::runtime_error("Parse error generated at " + loc.to_string())
        , location(loc)
    {}

    parse_error(std::size_t const line, std::size_t const col) noexcept
        : std::runtime_error((boost::format("Parse error generated at line:%1%, col:%2%") % line % col).str())
        , location()
    {
        location.line = line;
        location.col = col;
    }
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
                c.red("Error ")
                + c.bold(msg) + "\n"
                "1 error generated in " + generator_name
            )
    {}

    code_generation_error(std::string const& generator_name, boost::format const& format) noexcept
        : code_generation_error(generator_name, format.str())
    {}
};

} // namespace dachs

#endif    // DACHS_EXCEPTION_HPP_INCLUDED
