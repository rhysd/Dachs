#include <unordered_map>
#include <cstdint>
#include <memory>

#include <boost/format.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

#include "dachs/codegen/llvmir/ir_generator.hpp"
#include "dachs/codegen/llvmir/type_ir_generator.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

class llvm_ir_generator {
    using val = llvm::Value *const;

    std::unordered_map<symbol::var_symbol, val> symbol_to_value;
    std::unordered_map<scope::func_scope, llvm::Function *> scope_to_func;
    llvm::Module *module = nullptr;
    llvm::LLVMContext &context;
    llvm::IRBuilder<> builder;

    template<class Node, class String>
    void error(Node const& n, String const& msg)
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

    template<class Node, class T, class String>
    T check(Node const& n, T v, String const& error_msg)
    {
        if (!v) {
            error(n, error_msg);
        }
        return v;
    }

public:

    llvm_ir_generator(llvm::LLVMContext &c)
        : context(c)
        , builder(context)
    {}

    val generate(ast::node::primary_literal const& pl)
    {
        struct literal_visitor : public boost::static_visitor<val> {
            llvm::LLVMContext &context;
            ast::node::primary_literal const& pl;

            literal_visitor(llvm::LLVMContext &c, ast::node::primary_literal const& pl)
                : context(c), pl(pl)
            {}

            val operator()(char const c)
            {
                return llvm::ConstantInt::get(
                        generate_type_ir(pl->type, context),
                        static_cast<std::uint8_t const>(c), false
                    );
            }

            val operator()(double const d)
            {
                return llvm::ConstantFP::get(context, llvm::APFloat(d));
            }

            val operator()(bool const b)
            {
                return b ? llvm::ConstantInt::getTrue(context) : llvm::ConstantInt::getFalse(context);
            }

            val operator()(std::string const&)
            {
                throw not_implemented_error{pl, __FILE__, __func__, __LINE__, "string constant generation"};
            }

            val operator()(int const i)
            {
                return llvm::ConstantInt::get(
                        generate_type_ir(pl->type, context),
                        static_cast<std::int64_t const>(i), false
                    );
            }

            val operator()(unsigned int const ui)
            {
                return llvm::ConstantInt::get(
                        generate_type_ir(pl->type, context),
                        static_cast<std::uint64_t const>(ui), true
                    );
            }

        } visitor{context, pl};

        return check(pl, boost::apply_visitor(visitor, pl->value), "Failed to generate constant");
    }

    llvm::Module *generate(ast::node::program const& p)
    {
        module = new llvm::Module("Should replace this name with a file name", context);
        if (!module) {
            error(p, "Failed to generate a module");
        }

        // TODO: visit

        return module;
    }

    template<class T>
    val generate(std::shared_ptr<T> const& node)
    {
        throw not_implemented_error{node, __FILE__, __func__, __LINE__, node->to_string()};
    }
};

} // namespace detail

llvm::Module &generate_llvm_ir(ast::ast const& a, scope::scope_tree const&)
{
    // TODO:
    // Use global context temporarily
    return *detail::llvm_ir_generator{llvm::getGlobalContext()}.generate(a.root);
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
