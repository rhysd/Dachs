#if !defined DACHS_PARSER_IMPORTER_HPP_INCLUDED
#define      DACHS_PARSER_IMPORTER_HPP_INCLUDED

#include <vector>
#include <string>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/parser.hpp"

namespace dachs {
namespace syntax {

using dirs_type = std::vector<std::string>;

ast::node::inu &import(ast::node::inu &, dirs_type const&, parser const&, std::string const&);
ast::node::inu &&import(ast::node::inu &&, dirs_type const&, parser const&, std::string const&);

} // namespace parser
} // namespace dachs

#endif    // DACHS_PARSER_IMPORTER_HPP_INCLUDED
