#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>
#include <iostream>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {

class compiler final {
    syntax::parser parser;

    using files_type = std::vector<std::string>;

    std::string read(std::string const& file) const;

public:

    std::string compile(std::string const& file, files_type const& libdirs, bool const colorful = true, bool const debug = false) const;

    std::string report_ast(std::string const& file, std::string const& code, bool const colorful = true) const;
    std::string report_scope_tree(std::string const& file, std::string const& code) const;
    std::string report_llvm_ir(std::string const& file, std::string const& code) const;

    void dump_asts(std::ostream &out, files_type const& files, bool const colorful = true) const;
    void dump_scope_trees(std::ostream &out, files_type const& files) const;
    void dump_llvm_irs(std::ostream &out, files_type const& files) const;
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
