#include <cstdlib>
#include <iostream>
#include <sstream>

#include <llvm/Support/raw_ostream.h>

#include "dachs/compiler.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/semantics/stringize_scope_tree.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"

namespace dachs {
void compiler::compile(std::string const& code, bool const colorful)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    std::cout << ast::stringize_ast(ast, colorful)
                    + "\n\n=========Scope Tree=========\n\n"
                    + scope::stringize_scope_tree(scope_tree)
             << "\n\n=========LLVM IR=========\n\n";
    codegen::llvmir::emit_llvm_ir(ast, scope_tree).dump();
}

std::string compiler::dump_ast(std::string const& code, bool const colorful)
{
    return ast::stringize_ast(parser.parse(code), colorful);
}

std::string compiler::dump_scopes(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);
    return scope::stringize_scope_tree(scope_tree);
}

std::string compiler::dump_llvm_ir(std::string const& code)
{
    auto ast = parser.parse(code);
    auto scope_tree = semantics::analyze_semantics(ast);

    std::string result;
    llvm::raw_string_ostream raw_os{result};
    codegen::llvmir::emit_llvm_ir(ast, scope_tree).print(raw_os, nullptr);
    return result;
}

}  // namespace dachs
