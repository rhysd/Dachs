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
#include "dachs/semantics/type.hpp"
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
    using val = llvm::Value *;

    std::unordered_map<symbol::var_symbol, val> var_value_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    llvm::Module *module = nullptr;
    llvm::LLVMContext &context;
    llvm::IRBuilder<> builder;

    template<class Node>
    void error(Node const& n, boost::format const& msg)
    {
        error(n, msg.str());
    }

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

    template<class Node, class T>
    T check(Node const& n, T v, boost::format const& fmt)
    {
        return check(n, v, fmt.str());
    }

    template<class Node, class T, class String>
    T check(Node const& n, T v, String const& feature_name)
    {
        if (!v) {
            error(n, std::string{"Failed to emit "} + feature_name);
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

    boost::optional<val> lookup_var(symbol::var_symbol const& var)
    {
        auto const result = var_value_table.find(var);
        if (result == std::end(var_value_table)) {
            return boost::none;
        } else {
            return result->second;
        }
    }

    void emit_func_prototype(ast::node::function_definition const& func_def)
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

        // Note:
        // Use to_string() instead of mangling.
        auto func_ir = llvm::Function::Create(
                func_type_ir,
                llvm::Function::ExternalLinkage,
                // Note:
                // Simply use "main" because lli requires "main" function as entry point of the program
                scope->name == "main" ? "main" : scope->to_string(),
                module
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
    }

    // Note: Temporary
    val emit_builtin_binary_expression(std::string const& op, val const lhs, val const rhs, std::string const& lhs_type_name)
    {
        bool const is_float = lhs_type_name == "float";
        bool const is_int = lhs_type_name == "int";
        bool const is_uint = lhs_type_name == "uint";

        if (op == ">>") {
            return builder.CreateAShr(lhs, rhs, "shrtmp");
        } else if (op == "<<") {
            return builder.CreateShl(lhs, rhs, "shltmp");
        } else if (op == "*") {
            if (is_int || is_uint) {
                return builder.CreateMul(lhs, rhs, "multmp");
            } else if (is_float) {
                return builder.CreateFMul(lhs, rhs, "fmultmp");
            }
        } else if (op == "/") {
            if (is_int) {
                return builder.CreateSDiv(lhs, rhs, "sdivtmp");
            } else if (is_uint) {
                return builder.CreateUDiv(lhs, rhs, "udivtmp");
            } else if (is_float) {
                return builder.CreateFDiv(lhs, rhs, "fdivtmp");
            }
        } else if (op == "%") {
            if (is_int) {
                return builder.CreateSRem(lhs, rhs, "sremtmp");
            } else if (is_uint) {
                return builder.CreateURem(lhs, rhs, "uremtmp");
            } else if (is_float) {
                return builder.CreateFRem(lhs, rhs, "fremtmp");
            }
        } else if (op == "+") {
            if (is_int || is_uint) {
                return builder.CreateAdd(lhs, rhs, "addtmp");
            } else if (is_float) {
                return builder.CreateFAdd(lhs, rhs, "faddtmp");
            }
        } else if (op == "-") {
            if (is_int || is_uint) {
                return builder.CreateSub(lhs, rhs, "subtmp");
            } else if (is_float) {
                return builder.CreateFSub(lhs, rhs, "fsubtmp");
            }
        } else if (op == "&") {
            return builder.CreateAnd(lhs, rhs, "andtmp");
        } else if (op == "^") {
            return builder.CreateXor(lhs, rhs, "xortmp");
        } else if (op == "|") {
            return builder.CreateOr(lhs, rhs, "ortmp");
        } else if (op == "<") {
            if (is_int) {
                return builder.CreateICmpSLT(lhs, rhs, "icmpslttmp");
            } else if (is_uint) {
                return builder.CreateICmpULT(lhs, rhs, "icmpulttmp");
            } else if (is_float) {
                return builder.CreateFCmpULT(lhs, rhs, "fcmpulttmp");
            }
        } else if (op == ">") {
            if (is_int) {
                return builder.CreateICmpSGT(lhs, rhs, "icmpsgttmp");
            } else if (is_uint) {
                return builder.CreateICmpUGT(lhs, rhs, "icmpugttmp");
            } else if (is_float) {
                return builder.CreateFCmpUGT(lhs, rhs, "fcmpugttmp");
            }
        } else if (op == "<=") {
            if (is_int) {
                return builder.CreateICmpSLE(lhs, rhs, "icmpsletmp");
            } else if (is_uint) {
                return builder.CreateICmpULE(lhs, rhs, "icmpuletmp");
            } else if (is_float) {
                return builder.CreateFCmpULE(lhs, rhs, "fcmpuletmp");
            }
        } else if (op == ">=") {
            if (is_int) {
                return builder.CreateICmpSGE(lhs, rhs, "icmpsgetmp");
            } else if (is_uint) {
                return builder.CreateICmpUGE(lhs, rhs, "icmpugetmp");
            } else if (is_float) {
                return builder.CreateFCmpUGE(lhs, rhs, "fcmpugetmp");
            }
        } else if (op == "==") {
            if (is_int || is_uint) {
                return builder.CreateICmpEQ(lhs, rhs, "icmpeqtmp");
            } else if (is_float) {
                return builder.CreateFCmpUEQ(lhs, rhs, "fcmpeqtmp");
            }
        } else if (op == "!=") {
            if (is_int || is_uint) {
                return builder.CreateICmpNE(lhs, rhs, "icmpnetmp");
            } else if (is_float) {
                return builder.CreateFCmpUNE(lhs, rhs, "fcmpnetmp");
            }
        } else if (op == "&&") {
            if (!is_float) {
                return builder.CreateAnd(lhs, rhs, "andltmp");
            }
        } else if (op == "||") {
            if (!is_float) {
                return builder.CreateOr(lhs, rhs, "orltmp");
            }
        }

        return nullptr;
    }

    val emit_builtin_func(scope::func_scope const& builtin)
    {
        // TODO:
        return nullptr;
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
            llvm::IRBuilder<> &builder;

            literal_visitor(llvm::LLVMContext &c, ast::node::primary_literal const& pl, llvm::IRBuilder<> &b)
                : context(c), pl(pl), builder(b)
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

            val operator()(std::string const& s)
            {
                return builder.CreateGlobalStringPtr(s.c_str());
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

        } visitor{context, pl, builder};

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
        for (auto const& i : p->inu) {
            if (auto const maybe_func_def = get_as<ast::node::function_definition>(i)) {
                auto const& func_def = *maybe_func_def;

                if (func_def->is_template()) {
                    for (auto const& instantiated_func_def : func_def->instantiated) {
                        emit_func_prototype(instantiated_func_def);
                    }
                } else {
                    emit_func_prototype(func_def);
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

    void emit(ast::node::if_stmt const& if_)
    {
        auto *parent = builder.GetInsertBlock()->getParent();

        auto *then_block = check(if_, llvm::BasicBlock::Create(context, "if.then", parent), "then block");
        auto *else_block = check(if_, llvm::BasicBlock::Create(context, "if.else", parent), "else block");
        auto *end_block = check(if_, llvm::BasicBlock::Create(context, "if.end"), "end of block");

        // IR for if-then clause
        val cond_val = emit(if_->condition);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, builder.CreateNot(cond_val, "if_stmt_unless"), "unless statement");
        }
        builder.CreateCondBr(cond_val, then_block, else_block);
        builder.SetInsertPoint(then_block);
        emit(if_->then_stmts);
        builder.CreateBr(end_block);
        builder.SetInsertPoint(else_block);

        // IR for elseif clause
        for (auto const& elseif : if_->elseif_stmts_list) {
            then_block = check(if_, llvm::BasicBlock::Create(context, "if.then", parent), "then block");
            else_block = check(if_, llvm::BasicBlock::Create(context, "if.else", parent), "else block");
            cond_val = emit(elseif.first);
            builder.CreateCondBr(cond_val, then_block, else_block);
            builder.SetInsertPoint(then_block);
            emit(elseif.second);
            builder.CreateBr(end_block);
            builder.SetInsertPoint(else_block);
        }

        // IR for else clause
        if (if_->maybe_else_stmts) {
            builder.SetInsertPoint(else_block);
            emit(*if_->maybe_else_stmts);
        }

        parent->getBasicBlockList().push_back(end_block);
        builder.SetInsertPoint(end_block);
    }

    void emit(ast::node::return_stmt const& return_)
    {
        if (return_->ret_exprs.size() == 1) {
            builder.CreateRet(emit(return_->ret_exprs[0]));
        } else if (return_->ret_exprs.empty()) {
            // TODO:
            // Return statements with no expression in functions should returns unit
            builder.CreateRetVoid();
        } else {
            throw not_implemented_error{return_, __FILE__, __func__, __LINE__, "returning multiple value"};
        }
    }

    val emit(ast::node::func_invocation const& invocation)
    {
        std::vector<val> args;
        args.reserve(invocation->args.size());
        for (auto const& a : invocation->args) {
            args.push_back(emit(a));
        }

        assert(!invocation->func_symbol.expired());
        auto scope = invocation->func_symbol.lock();
        if (scope->is_builtin) {
            return check(invocation, emit_builtin_func(scope), "Built-in function");
        }

        auto *const callee = module->getFunction(scope->to_string());

        // TODO
        // Monad invocation

        return builder.CreateCall(callee, args, "funcinvotmp");
    }

    val emit(ast::node::binary_expr const& bin_expr)
    {
        auto const lhs_type = type::type_of(bin_expr->lhs);
        auto const rhs_type = type::type_of(bin_expr->rhs);

        if (!lhs_type.is_builtin() || !rhs_type.is_builtin()) {
            error(bin_expr, "Binary expression now only supports float, int, bool and uint");
        }

        auto const lhs_builtin_type = *type::get<type::builtin_type>(lhs_type);
        auto const rhs_builtin_type = *type::get<type::builtin_type>(rhs_type);
        auto const is_supported = [](auto const& t){ return t->name == "int" || t->name == "float" || t->name == "uint" || t->name == "bool"; };

        if (!is_supported(lhs_builtin_type) || !is_supported(rhs_builtin_type)) {
            error(bin_expr, "Binary expression now only supports float, int and uint");
        }

        return check(
            bin_expr,
            emit_builtin_binary_expression(bin_expr->op, emit(bin_expr->lhs), emit(bin_expr->rhs), lhs_builtin_type->name),
            boost::format("binary operator '%1%' (rhs type is '%2%', lhs type is '%3%')")
                % bin_expr->op
                % lhs_builtin_type->to_string()
                % rhs_builtin_type->to_string()
        );
    }

    val emit(ast::node::var_ref const& var)
    {
        assert(!var->symbol.expired());
        auto const maybe_var_value = lookup_var(var->symbol.lock());
        assert(maybe_var_value);
        return *maybe_var_value;
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
