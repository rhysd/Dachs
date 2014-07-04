#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <llvm/Support/raw_ostream.h>

#include "dachs/compiler.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/semantics/stringize_scope_tree.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/executable_generator.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {

std::string compiler::read(std::string const& file) const
{
    auto const maybe_code = dachs::helper::read_file<std::string>(file);
    if (!maybe_code) {
        throw std::runtime_error{"File cannot be opened: " + file};
    }
    return *maybe_code;
}

std::string compiler::compile(std::string const& file, std::vector<std::string> const& libdirs, bool const colorful, bool const debug) const
{
    auto const code = read(file);

    auto ast = parser.parse(code, file);
    auto scope_tree = semantics::analyze_semantics(ast);
    std::cout << ast::stringize_ast(ast, colorful)
                    + "\n\n=========Scope Tree=========\n\n"
                    + scope::stringize_scope_tree(scope_tree)
             << "\n\n=========LLVM IR=========\n\n";
    auto &module = codegen::llvmir::emit_llvm_ir(ast, scope_tree);
    module.dump();

    return codegen::llvmir::generate_executable(module, file, libdirs);
}

std::string compiler::report_ast(std::string const& file, std::string const& code, bool const colorful) const
{
    return ast::stringize_ast(parser.parse(code, file), colorful);
}

std::string compiler::report_scope_tree(std::string const& file, std::string const& code) const
{
    auto ast = parser.parse(code, file);
    auto scope_tree = semantics::analyze_semantics(ast);
    return scope::stringize_scope_tree(scope_tree);
}

std::string compiler::report_llvm_ir(std::string const& file, std::string const& code) const
{
    auto ast = parser.parse(code, file);
    auto scope_tree = semantics::analyze_semantics(ast);

    std::string result;
    llvm::raw_string_ostream raw_os{result};
    codegen::llvmir::emit_llvm_ir(ast, scope_tree).print(raw_os, nullptr);
    return result;
}

void compiler::dump_asts(std::ostream &out, compiler::files_type const& files, bool const colorful) const
{
   for (auto const& f : files) {
       out << "file: " << f << '\n'
           << report_ast(f, read(f), colorful) << '\n';
   }
}

void compiler::dump_scope_trees(std::ostream &out, compiler::files_type const& files) const
{
   for (auto const& f : files) {
       out << "file: " << f << '\n'
           << report_scope_tree(f, read(f)) << '\n';
   }
}

void compiler::dump_llvm_irs(std::ostream &out, compiler::files_type const& files) const
{
   for (auto const& f : files) {
       out << "file: " << f << '\n'
           << report_llvm_ir(f, read(f)) << '\n';
   }
}

}  // namespace dachs
