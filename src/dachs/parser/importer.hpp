#if !defined DACHS_PARSER_IMPORTER_HPP_INCLUDED
#define      DACHS_PARSER_IMPORTER_HPP_INCLUDED

#include <set>
#include <vector>
#include <string>

#include <boost/filesystem/path.hpp>

#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace syntax {

namespace fs = boost::filesystem;

using dirs_type = std::vector<std::string>;

struct importer {
    dirs_type const& import_dirs;
    fs::path source;
    std::set<fs::path> already_imported;

    template<class Source>
    importer(dirs_type const& is, Source const& s)
        : import_dirs(is), source(s), already_imported()
    {}

    ast::node::inu const& import(ast::node::inu const&);
};

} // namespace parser
} // namespace dachs

#endif    // DACHS_PARSER_IMPORTER_HPP_INCLUDED
