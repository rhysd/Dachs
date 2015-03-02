#include <iostream>
#include <string>
#include <set>

#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace syntax {

namespace detail {

namespace fs = boost::filesystem;
namespace algo = boost::algorithm;

using boost::adaptors::transformed;

class importer final {

    using maybe_path = boost::optional<fs::path>;

    dirs_type const& import_dirs;
    parser const& file_parser;
    helper::colorizer c;
    fs::path source_file;
    std::set<fs::path> already_imported;

    template<class Node, class Message>
    [[noreturn]]
    void error(Node const& node, fs::path const& file, Message const& msg)
    {
        std::cerr << c.red("Error") << " while importing " << file << '\n'
                  << "At line:" << node->line << ", col:" << node->col << ". " << msg << std::endl;
        throw parse_error{node->line, node->col};
    }

    template<class Message>
    void report(ast::node::import const& i, Message const& msg)
    {
        std::cerr << c.red("Error") << " while importing '" << i->path << "' at line:" << i->line << ", col:" << i->col << '\n'
                  << msg << std::endl;
    }

    template<class Message>
    [[noreturn]]
    void error(ast::node::import const& i, Message const& msg)
    {
        report(i, msg);
        throw parse_error{i->line, i->col};
    }

    ast::ast &merge(ast::ast &lhs, ast::ast &&rhs)
    {
        auto const merge_vector
            = [](auto &l, auto &&r)
            {
                l.insert(
                    std::end(l),
                    std::make_move_iterator(std::begin(r)),
                    std::make_move_iterator(std::end(r))
                );
            };

        auto const& inu = lhs.root;
        merge_vector(inu->functions, std::move(rhs.root->functions));
        merge_vector(inu->global_constants, std::move(rhs.root->global_constants));
        merge_vector(inu->classes, std::move(rhs.root->classes));

        return lhs;
    }

    fs::path find_path(ast::node::import const& node)
    {
        fs::path const specified_path
            = algo::replace_all_copy(node->path, ".", "/") + ".dcs";

        auto const search
            = [&specified_path, this](auto const& name) -> maybe_path
            {
                auto p = fs::path{name} / specified_path;
                if (!fs::exists(p)) {
                    p = source_file.parent_path() / p;
                }

                if (fs::is_directory(p)) {
                    return boost::none; // Note: Give up
                }

                if (fs::exists(p)) {
                    return p;
                } else {
                    return boost::none;
                }
            };

        // Note: Check system library
        if (auto const found = search(DACHS_INSTALL_PREFIX "/lib/dachs")) {
            return *found;
        }

        // Note: Check specified dir
        for (auto const& d : import_dirs
                | transformed([](auto const& d){ return fs::path{d}; })) {
            if (auto const found = search(d)) {
                return *found;
            }
        }

        // Note: Check source's dir
        if (auto const found = search(source_file.parent_path())) {
            return *found;
        }

        std::string notes =
                "  Note: Import directories are below\n"
                "    System:    " DACHS_INSTALL_PREFIX "/lib/dachs";
        for (auto const& i : import_dirs) {
            notes += "\n    Specified: " + i;
        }
        notes += "\n    Relative:  " + source_file.parent_path().string();

        error(node, "  File \"" + specified_path.string() + "\" is not found in any import paths\n" + notes);
    }

    ast::ast parse(ast::node::import const& i, fs::path const& p, std::string const& code, fs::path const& f)
    {
        try {
            auto ast = file_parser.parse(code, p.c_str());
            this->import(ast, p);
            return ast;
        } catch(parse_error const& err) {
            report(i, boost::format(
                        "  Error occurred while parsing imported file %1%\n"
                        "  Note: Imported from file %2%"
                    ) % p % f);
            throw err;
        }
    }

public:

    importer(dirs_type const& dirs, parser const& p, std::string const& source)
        : import_dirs(dirs), file_parser(p), source_file(source), already_imported()
    {
        if (!source_file.has_root_directory()) {
            source_file = fs::current_path() / source_file;
        }
    }

    ast::ast &import(ast::ast &program)
    {
        return import(program, source_file);
    }

    ast::ast &import(ast::ast &program, fs::path const& file)
    {
        already_imported.insert(file);

        for (auto &i : program.root->imports) {
            auto const p = find_path(i);
            if (already_imported.find(p) != std::end(already_imported)) {
                continue;
            }

            auto const source = helper::read_file(p.c_str());
            if (!source) {
                error(i, boost::format("  Can't open file %1%") % p);
            }

            merge(program, parse(i, p, *source, file));
        }

        return program;
    }
};

} // namespace detail

ast::ast &import(ast::ast &a, dirs_type const& dirs, parser const& p, std::string const& file)
{
    detail::importer importer{dirs, p, file};
    return importer.import(a);
}

ast::ast &&import(ast::ast &&a, dirs_type const& dirs, parser const& p, std::string const& file)
{
    import(a, dirs, p, file);
    return std::move(a);
}

} // namespace parser
} // namespace dachs
