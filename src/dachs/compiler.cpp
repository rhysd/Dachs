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

namespace dachs {

void compiler::compile(std::string const& file, std::string const& code, std::vector<std::string> const& libdirs, bool const colorful) const
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    std::cout << ast::stringize_ast(ast, colorful)
                    + "\n\n=========Scope Tree=========\n\n"
                    + scope::stringize_scope_tree(scope_tree)
             << "\n\n=========LLVM IR=========\n\n";
    auto &module = codegen::llvmir::emit_llvm_ir(ast, scope_tree);
    module.dump();
    codegen::llvmir::generate_executable(module, file, libdirs);
}

std::string compiler::dump_ast(std::string const& code, bool const colorful) const
{
    return ast::stringize_ast(parser.parse(code), colorful);
}

std::string compiler::dump_scopes(std::string const& code) const
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    return scope::stringize_scope_tree(scope_tree);
}

std::string compiler::dump_llvm_ir(std::string const& code) const
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);

    std::string result;
    llvm::raw_string_ostream raw_os{result};
    codegen::llvmir::emit_llvm_ir(ast, scope_tree).print(raw_os, nullptr);
    return result;
}

}  // namespace dachs
