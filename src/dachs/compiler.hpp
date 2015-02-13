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

    explicit compiler(bool const colorful);

    std::string compile(files_type const& files, files_type const& libdirs, bool const debug = false) const;
    std::vector<std::string> compile_to_objects(files_type const& files, bool const debug = false) const;

    std::string report_ast(std::string const& file, std::string const& code) const;
    std::string report_scope_tree(std::string const& file, std::string const& code) const;
    std::string report_llvm_ir(std::string const& file, std::string const& code) const;
    bool check_syntax(std::vector<std::string> const& files) const;

    void dump_asts(std::ostream &out, files_type const& files) const;
    void dump_scope_trees(std::ostream &out, files_type const& files) const;
    void dump_llvm_irs(std::ostream &out, files_type const& files) const;
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
