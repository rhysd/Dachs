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

    void /*TEMPORARY*/ compile(std::string const& file, std::string const& code, std::vector<std::string> const& libdirs, bool const colorful = true) const;

    std::string dump_ast(std::string const& code, bool const colorful = true) const;
    std::string dump_scopes(std::string const& code) const;
    std::string dump_llvm_ir(std::string const& code) const;
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
