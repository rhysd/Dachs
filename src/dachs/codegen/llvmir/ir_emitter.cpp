#include <unordered_map>
#include <memory>
#include <utility>
#include <cstdint>
#include <cassert>

#include <boost/format.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/Verifier.h>

#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"

/*
 * Note:
 * When code generator is too big to manage one class,
 * separate visitors to each nodes to individual classes.
 * I think they are specialization of class template
 *
 * template<class T>
 * class code_generator : code_generator_base; // If need default implementation, implement it as this primary class
 *
 * template<>
 * class code_generator<ast::node::program> : code_generator_base { ... };
 *
 * template<>
 * class code_generator<ast::node::function_definition> : code_generator_base { ... };
 *
 * ...
 *
 *
 * These specialized class template visitors can be separated to individual transform units.
 *
 * class codegen_walker {
 *     llvm::LLVMContext context;
 *     llvm::IRBuilder<> builder;
 *
 * public:
 *
 *     template<class T, class W>
 *     void visit(std::shared_ptr<T> const& n, W const& walker)
 *     {
 *         code_generator<T> generator{n, context, builder};
 *         generator.emit_before();
 *         walker();
 *         generator.emit_after();
 *     }
 * };
 */

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

using helper::variant::apply_lambda;
using helper::variant::get_as;

class llvm_ir_emitter {
    using val = llvm::Value *const;

    std::unordered_map<symbol::var_symbol, val> var_value_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
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
    T check(Node const& n, T v, String const& feature_name)
    {
        if (!v) {
            error(n, std::string{"Failed to emit"} + feature_name);
        }
        return v;
    }

    template<class... NodeTypes>
    auto emit(boost::variant<NodeTypes...> const& ns)
    {
        return apply_lambda([this](auto const& n){ return emit(n); }, ns);
    }

    template<class Node>
    auto emit(boost::optional<Node> const& o)
        -> boost::optional<decltype(emit(std::declval<Node const&>()))>
    {
        return o ? emit(*o) : boost::none;
    }

    boost::optional<llvm::Function *> lookup_func(scope::func_scope const& scope)
    {
        auto const result = func_table.find(scope);
        if (result == std::end(func_table)) {
            return boost::none;
        } else {
            return result->second;
        }
    }

public:

    llvm_ir_emitter(llvm::LLVMContext &c)
        : context(c)
        , builder(context)
    {}

    val emit(ast::node::primary_literal const& pl)
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
                        emit_type_ir(pl->type, context),
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
                        emit_type_ir(pl->type, context),
                        static_cast<std::int64_t const>(i), false
                    );
            }

            val operator()(unsigned int const ui)
            {
                return llvm::ConstantInt::get(
                        emit_type_ir(pl->type, context),
                        static_cast<std::uint64_t const>(ui), true
                    );
            }

        } visitor{context, pl};

        return check(pl, boost::apply_visitor(visitor, pl->value), "constant");
    }

    llvm::Module *emit(ast::node::program const& p)
    {
        module = new llvm::Module("Should replace this name with a file name", context);
        if (!module) {
            error(p, "module");
        }

        // Note:
        // emit Function prototypes in advance for forward reference

        auto const emit_prototype =
            [this](ast::node::function_definition const& func_def)
            {
                assert(!func_def->scope.expired());
                std::vector<llvm::Type *> param_type_irs;
                param_type_irs.reserve(func_def->params.size());
                auto const scope = func_def->scope.lock();

                for (auto const& param_sym : scope->params) {
                    param_type_irs.push_back(emit_type_ir(param_sym->type, context));
                }

                auto func_type_ir = llvm::FunctionType::get(
                        emit_type_ir(*func_def->ret_type, context),
                        param_type_irs,
                        false
                    );

                check(func_def, func_type_ir, "function type");

                auto func_ir = llvm::Function::Create(
                        func_type_ir,
                        llvm::Function::ExternalLinkage,
                        func_def->name, module
                    );

                check(func_def, func_type_ir, "function");

                {
                    auto arg_itr = func_ir->arg_begin();
                    auto param_itr = std::begin(scope->params);
                    for (; param_itr != std::end(scope->params); ++arg_itr, ++param_itr) {
                        arg_itr->setName((*param_itr)->name);
                        var_value_table.insert(std::make_pair(*param_itr, arg_itr));
                    }
                }

                func_table.insert(std::make_pair(scope, func_ir));
            };

        for (auto const& i : p->inu) {
            if (auto const maybe_func_def = get_as<ast::node::function_definition>(i)) {
                auto const& func_def = *maybe_func_def;

                if (func_def->is_template()) {
                    for (auto const& instantiated_func_def : func_def->instantiated) {
                        emit_prototype(instantiated_func_def);
                    }
                } else {
                    emit_prototype(func_def);
                }
            }
        }

        for (auto const& i : p->inu) {
            emit(i);
        }

        return module;
    }

    // Note:
    // IR for the function is already emitd in emit(ast::node::program const&)
    void emit(ast::node::function_definition const& func_def)
    {
        if (func_def->is_template()) {
            for (auto const& i : func_def->instantiated) {
                emit(i);
            }
            return;
        }

        // Note:
        // It is no need to visit params because it is already visited
        // in emit(ast::node::program const&) because of forward reference

        // Note: Already checked the scope is not empty
        auto maybe_prototype_ir = lookup_func(func_def->scope.lock());
        assert(maybe_prototype_ir);
        auto &prototype_ir = *maybe_prototype_ir;
        auto const block = llvm::BasicBlock::Create(context, "entry", prototype_ir);
        builder.SetInsertPoint(block);

        emit(func_def->body);

        auto const unit_type = type::make<type::tuple_type>();
        if (!func_def->ret_type || func_def->kind == ast::symbol::func_kind::proc) {
            builder.CreateRetVoid();
        }
    }

    void emit(ast::node::statement_block const& block)
    {
        // Basic block is already emitd on visiting function_definition and for_stmt
        for (auto const& stmt : block->value) {
            emit(stmt);
        }
    }

    template<class T>
    val emit(std::shared_ptr<T> const& node)
    {
        throw not_implemented_error{node, __FILE__, __func__, __LINE__, node->to_string()};
    }
};

} // namespace detail

llvm::Module &emit_llvm_ir(ast::ast const& a, scope::scope_tree const&)
{
    // TODO:
    // Use global context temporarily
    // TODO
    // Verify IR using VerifyModule() and VerifierPass class
    return *detail::llvm_ir_emitter{llvm::getGlobalContext()}.emit(a.root);
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
