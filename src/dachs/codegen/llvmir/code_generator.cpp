#include <unordered_map>

#include <llvm/IR/Module.h>

#include "dachs/codegen/llvmir/code_generator.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

struct llvm_ir_generator {
    std::unordered_map<symbol::var_symbol, llvm::Value *> symbol_to_value;
    std::unordered_map<scope::func_scope, llvm::Function *> scope_to_func;
    llvm::Module *module;

    llvm::Module *generate(ast::node::program const&)
    {
        return nullptr;
    }
};

} // namespace detail

llvm::Module &generate_llvm_ir(ast::ast const& a, scope::scope_tree const& t)
{
    auto mod = detail::llvm_ir_generator{}.generate(a.root);
    if (!mod) {
        // TODO
    }
    return *mod;
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
