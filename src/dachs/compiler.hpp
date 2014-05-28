#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {

class compiler final {
    syntax::parser parser;
public:

    void /*TEMPORARY*/ compile(std::string const& code);

    std::string dump_ast(std::string const& code, bool const colorful = true);
    std::string dump_scopes(std::string const& code);
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
