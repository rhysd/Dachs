#include <unordered_map>

#include <boost/format.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "dachs/codegen/llvmir/code_generator.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

class llvm_ir_generator {
    std::unordered_map<symbol::var_symbol, llvm::Value *> symbol_to_value;
    std::unordered_map<scope::func_scope, llvm::Function *> scope_to_func;
    llvm::Module *module = nullptr;
    llvm::LLVMContext &context;
    llvm::IRBuilder<> builder;

    template<class Node>
    void error(Node const& n, std::string const& msg)
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (boost::format("In line:%1%:col:%2%, %3%") % n->line % n->col % msg).str()
            };
    }

    template<class Node>
    void error(Node const& n, boost::format const& msg)
    {
        error(n, msg.str());
    }

public:

    llvm_ir_generator()
        : context(llvm::getGlobalContext())
        , builder(context) // Note: Use global context temporarily
    {}

    llvm_ir_generator(llvm::LLVMContext &c)
        : context(c)
        , builder(context)
    {}

    llvm::Module *generate(ast::node::program const& p)
    {
        module = new llvm::Module("Should replace this name with a file name", context);
        if (!module) {
            error(p, "Failed to generate a module");
        }

        // TODO: visit

        return module;
    }
};

} // namespace detail

llvm::Module &generate_llvm_ir(ast::ast const& a, scope::scope_tree const&)
{
    auto module = detail::llvm_ir_generator{}.generate(a.root);
    return *module;
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
