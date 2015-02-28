#include <iostream>
#include <string>

#include <boost/format.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {
namespace syntax {

namespace detail {

class importer final {

    dirs_type const& import_dirs;
    parser const& file_parser;
    helper::colorizer c;

    template<class Node, class Message>
    [[noreturn]]
    void error(Node const& node, std::string const& file, Message const& msg)
    {
        std::cerr << c.red("Error") << " while importing " << file << '\n'
                  << msg << std::endl;
    }

public:

    importer(dirs_type const& dirs, parser const& p) noexcept
        : import_dirs(dirs), file_parser(p)
    {}

    ast::node::inu &merge(ast::node::inu &lhs, ast::node::inu &&rhs)
    {
        return lhs;
    }

    ast::node::inu &import(ast::node::inu &i, std::string const& file)
    {
        return i;
    }
};

} // namespace detail

ast::node::inu &import(ast::node::inu &i, dirs_type const& dirs, parser const& p, std::string const& file)
{
    detail::importer importer{dirs, p};
    return importer.import(i, file);
}

ast::node::inu &&import(ast::node::inu &&i, dirs_type const& dirs, parser const& p, std::string const& file)
{
    import(i, dirs, p, file);
    return std::move(i);
}

} // namespace parser
} // namespace dachs
