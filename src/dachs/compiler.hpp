#if !defined DACHS_COMPILER_HPP_INCLUDED
#define      DACHS_COMPILER_HPP_INCLUDED

#include <string>
#include <iostream>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/codegen/opt_level.hpp"

namespace dachs {

class compiler final {
    syntax::parser parser;
    bool debug;
    codegen::opt_level opt;

    using files_type = std::vector<std::string>;

    std::string read(std::string const& file) const;

public:

    compiler(bool const colorful, bool const debug, codegen::opt_level const opt = codegen::opt_level::none);

    std::string compile(
            files_type const& files,
            files_type const& libdirs,
            files_type const& importdirs,
            std::string parent = ""
        ) const;

    std::vector<std::string> compile_to_objects(
            files_type const& files,
            files_type const& importdirs,
            std::string parent = ""
        ) const;

    std::string report_ast(std::string const& file, std::string const& code) const;
    std::string report_scope_tree(std::string const& file, std::string const& code, files_type const& importdirs) const;
    std::string report_llvm_ir(std::string const& file, std::string const& code, files_type const& importdirs) const;
    bool check_syntax(std::vector<std::string> const& files) const;

    void dump_asts(std::ostream &out, files_type const& files) const;
    void dump_scope_trees(std::ostream &out, files_type const& files, files_type const& importdirs) const;
    void dump_llvm_irs(std::ostream &out, files_type const& files, files_type const& importdirs) const;
};

} // namespace dachs
#endif    // DACHS_COMPILER_HPP_INCLUDED
