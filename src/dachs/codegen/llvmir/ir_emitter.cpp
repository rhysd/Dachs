#include <unordered_map>
#include <memory>
#include <utility>
#include <string>
#include <iostream>
#include <stack>
#include <cstdint>
#include <cassert>

#include <boost/format.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/Analysis/Verifier.h>

#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/codegen/llvmir/tmp_builtin_operator_ir_emitter.hpp"
#include "dachs/codegen/llvmir/builtin_func_ir_emitter.hpp"
#include <dachs/codegen/llvmir/variable_table.hpp>
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/each.hpp"

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

    llvm::Module *module = nullptr;
    llvm::LLVMContext &context;
    llvm::IRBuilder<> builder;
    variable_table var_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    builtin_function_emitter builtin_func_emitter;
    std::string const& file;
    std::stack<llvm::BasicBlock *> loop_stack; // Loop stack for continue and break statements

    auto push_loop(llvm::BasicBlock *loop_value)
    {
        loop_stack.push(loop_value);

        struct automatic_popper {
            std::stack<llvm::BasicBlock *> &s;
            llvm::BasicBlock *const pushed_loop;
            automatic_popper(decltype(s) &s, llvm::BasicBlock *const loop)
                : s(s), pushed_loop(loop)
            {}
            ~automatic_popper()
            {
                assert(!s.empty());
                assert(s.top() == pushed_loop);
                s.pop();
            }
        } popper{loop_stack, loop_value};
        return popper;
    }

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
    T check(Node const& n, T const v, boost::format const& fmt)
    {
        return check(n, v, fmt.str());
    }

    template<class Node, class T, class String>
    T check(Node const& n, T const v, String const& feature_name)
    {
        if (!v) {
            error(n, std::string{"Failed to emit "} + feature_name);
        }
        return v;
    }

    template<class Node, class String, class... Values>
    void check_all(Node const& n, String const& feature, Values const... vs)
    {
        static_assert(helper::are_same<Values...>::value, "check_all(): Type of values are invalid");
        // Note:
        // We can't use any_of() because template 'Range' can't be infered.
        for (auto const v : {vs...}) {
            if (!v) {
                error(n, std::string{"Failed to emit "} + feature);
            }
        }
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
                false // Non-variadic
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
                var_table.insert(*param_itr, arg_itr);
            }
        }

        func_table.emplace(scope, func_ir);
    }

public:

    llvm_ir_emitter(llvm::LLVMContext &c, std::string const& f)
        : context(c)
        , builder(context)
        , var_table(builder)
        , builtin_func_emitter(context)
        , file(f)
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

    val emit(ast::node::symbol_literal const& sym)
    {
        return check(sym, builder.CreateGlobalStringPtr(sym->value.c_str()), "symbol constant");
    }

    llvm::Module *emit(ast::node::program const& p)
    {
        module = new llvm::Module(file, context);
        if (!module) {
            error(p, "module");
        }

        builtin_func_emitter.set_module(module);

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

        assert(loop_stack.empty());

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

        if (builder.GetInsertBlock()->getTerminator()) {
            return;
        }

        if (!func_def->ret_type
                || func_def->kind == ast::symbol::func_kind::proc
                || *func_def->ret_type == type::get_unit_type()) {
            builder.CreateRetVoid();
        } else {
            // Note:
            // Believe that the insert block is the last block of the function
            builder.CreateUnreachable();
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

        auto *then_block = llvm::BasicBlock::Create(context, "if.then", parent);
        auto *else_block = llvm::BasicBlock::Create(context, "if.else", parent);
        auto *end_block  = llvm::BasicBlock::Create(context, "if.end");
        check_all(if_, "if statement", then_block, else_block, end_block);

        // IR for if-then clause
        val cond_val = emit(if_->condition);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, builder.CreateNot(cond_val, "if_stmt_unless"), "unless statement");
        }
        builder.CreateCondBr(cond_val, then_block, else_block);
        builder.SetInsertPoint(then_block);
        emit(if_->then_stmts);
        if (!then_block->getTerminator()) {
            builder.CreateBr(end_block);
        }
        builder.SetInsertPoint(else_block);

        // IR for elseif clause
        for (auto const& elseif : if_->elseif_stmts_list) {
            cond_val = emit(elseif.first);
            then_block = llvm::BasicBlock::Create(context, "if.then", parent);
            else_block = llvm::BasicBlock::Create(context, "if.else", parent);
            check_all(if_, "elseif clause", then_block, else_block);
            builder.CreateCondBr(cond_val, then_block, else_block);
            builder.SetInsertPoint(then_block);
            emit(elseif.second);
            if (!then_block->getTerminator()) {
                builder.CreateBr(end_block);
            }
            builder.SetInsertPoint(else_block);
        }

        // IR for else clause
        if (if_->maybe_else_stmts) {
            emit(*if_->maybe_else_stmts);
        }
        if (!else_block->getTerminator()) {
            builder.CreateBr(end_block);
        }

        parent->getBasicBlockList().push_back(end_block);
        builder.SetInsertPoint(end_block);
    }

    void emit(ast::node::return_stmt const& return_)
    {
        if (builder.GetInsertBlock()->getTerminator()) {
            // Note:
            // Basic block is already terminated.
            // Unreachable return statements should be checked in semantic checks
            return;
        }

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
            // Note:
            // scope->params is not available because builtin funcitons remains as function templates
            std::vector<type::type> param_types;
            param_types.reserve(invocation->args.size());
            for (auto const& e : invocation->args) {
                param_types.push_back(type::type_of(e));
            }

            auto callee = check(invocation, builtin_func_emitter.emit(scope->name, param_types), boost::format("builtin function '%1%'") % scope->name);
            return builder.CreateCall(callee, args);
        }

        auto *const callee = module->getFunction(scope->to_string());

        // TODO
        // Monad invocation

        return builder.CreateCall(callee, args);
    }

    val emit(ast::node::unary_expr const& unary)
    {
        auto const val_type = type::type_of(unary->expr);
        if (!val_type.is_builtin()) {
            error(unary, "Unary expression now only supports float, int, bool and uint");
        }

        auto const builtin = *type::get<type::builtin_type>(val_type);
        auto const& name = builtin->name;

        if (name != "int" && name != "float" && name != "bool" && name != "uint") {
            error(unary, "Unary expression now only supports float, int, bool and uint");
        }

        return check(
            unary,
            tmp_builtin_unary_op_ir_emitter{builder, emit(unary->expr), unary->op}.emit(builtin),
            boost::format("unary operator '%1%' (operand's type is '%2%')")
                % unary->op
                % builtin->to_string()
        );
    }

    bool is_available_type_for_binary_expression(type::type const& lhs, type::type const& rhs) const noexcept
    {
        if (!lhs.is_builtin() || !rhs.is_builtin()) {
            return false;
        }

        auto const lhs_builtin_type = *type::get<type::builtin_type>(lhs);
        auto const rhs_builtin_type = *type::get<type::builtin_type>(rhs);
        auto const is_supported = [](auto const& t){ return t->name == "int" || t->name == "float" || t->name == "uint" || t->name == "bool"; };

        return !is_supported(lhs_builtin_type) || !is_supported(rhs_builtin_type);
    }

    val emit(ast::node::binary_expr const& bin_expr)
    {
        auto const lhs_type = type::type_of(bin_expr->lhs);
        auto const rhs_type = type::type_of(bin_expr->rhs);

        if (!is_available_type_for_binary_expression(lhs_type, rhs_type)) {
            error(bin_expr, "Binary expression now only supports float, int, bool and uint");
        }

        return check(
            bin_expr,
            tmp_builtin_bin_op_ir_emitter{builder, emit(bin_expr->lhs), emit(bin_expr->rhs), bin_expr->op}.emit(lhs_type),
            boost::format("binary operator '%1%' (rhs type is '%2%', lhs type is '%3%')")
                % bin_expr->op
                % type::to_string(lhs_type)
                % type::to_string(rhs_type)
        );
    }

    val emit(ast::node::var_ref const& var)
    {
        assert(!var->symbol.expired());
        return check(var, var_table.emit_ir_to_load(var->symbol.lock()), "loading variable");
    }

    void emit(ast::node::while_stmt const& while_)
    {
        auto *const parent = builder.GetInsertBlock()->getParent();
        assert(parent);

        auto *const cond_block = llvm::BasicBlock::Create(context, "while.cond", parent);
        auto *const body_block = llvm::BasicBlock::Create(context, "while.body", parent);
        auto *const exit_block = llvm::BasicBlock::Create(context, "while.exit", parent);
        check_all(while_, "for statement", cond_block, body_block, exit_block);

        // Loop header
        builder.CreateBr(cond_block);
        builder.SetInsertPoint(cond_block);
        val cond_val = emit(while_->condition);
        builder.CreateCondBr(cond_val, body_block, exit_block);

        // Loop body
        auto const auto_popper = push_loop(cond_block);
        builder.SetInsertPoint(body_block);
        emit(while_->body_stmts);
        if (!body_block->getTerminator()) {
            builder.CreateBr(cond_block);
        }

        builder.SetInsertPoint(exit_block);
    }

    void emit(ast::node::initialize_stmt const& init)
    {
        auto const initializee_size = init->var_decls.size();
        auto const initializer_size
            = init->maybe_rhs_exprs ?
                init->maybe_rhs_exprs->size() : 0;

        assert(initializee_size != 0);
        assert(initializer_size != 0);

        auto const emit_alloca_from_decl
            = [&](auto const& decl)
            {
                assert(!decl->symbol.expired());
                auto sym = decl->symbol.lock();
                auto const t = sym->type;
                assert(t);
                auto inst = builder.CreateAlloca(emit_type_ir(t, context));

                var_table.insert(std::move(sym), inst);

                return check(decl, inst, "variable allocation");
            };

        if (initializee_size == initializer_size) {
            helper::each(
                    [&, this](auto const& d, auto const& e)
                    {
                        auto inst = emit_alloca_from_decl(d);
                        auto val = emit(e);
                        builder.CreateStore(val, inst);
                    }
                    , init->var_decls, *init->maybe_rhs_exprs);
            for (auto const& d : init->var_decls) {
                emit_alloca_from_decl(d);
            }
        } else if (initializee_size == 1) {
            assert(initializer_size > 1);

            // TODO:
            // Get initializee's type (tuple) and emit IR
            //  1. Allocate tuple
            //  2. Assign rhs's values to fields of the tuple
            //  3. Store the tuple value as lhs

            throw not_implemented_error{init, __FILE__, __func__, __LINE__, "multiple to one assignment"};
        } else if (initializer_size == 1) {
            assert(initializee_size > 1);

            // TODO:
            // Get the rhs types, access the elements and store the values to the initializees

            throw not_implemented_error{init, __FILE__, __func__, __LINE__, "one to multiple assignment"};
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    void emit(ast::node::assignment_stmt const& assign)
    {
        assert(assign->op.back() == '=');

        // Load rhs value
        std::vector<val> rhs_values;

        auto const assignee_size = assign->assignees.size();
        auto const assigner_size = assign->rhs_exprs.size();
        assert(assignee_size > 0 && assigner_size > 0);

        if (assignee_size == assigner_size) {
            helper::each(
                    [&](auto const& lhs, auto const& rhs)
                    {
                        if (!is_available_type_for_binary_expression(type::type_of(lhs), type::type_of(rhs))) {
                            error(assign, "Binary expression now only supports float, int, bool and uint");
                        }
                        rhs_values.push_back(emit(rhs));
                    }, assign->assignees, assign->rhs_exprs);
        } else if (assignee_size == 1) {
            throw not_implemented_error{assign, __FILE__, __func__, __LINE__, "multiple to one assignment"};
        } else if (assigner_size == 1) {
            throw not_implemented_error{assign, __FILE__, __func__, __LINE__, "one to multiple assignment"};
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        assert(assignee_size == rhs_values.size());

        helper::each(
            [&, this](auto const& lhs_expr, auto const rhs_value)
            {
                if (auto const maybe_var_ref = get_as<ast::node::var_ref>(lhs_expr)) {
                    assert(!(*maybe_var_ref)->symbol.expired());
                    auto const lhs_sym = (*maybe_var_ref)->symbol.lock();

                    if (assign->op == "=") {
                        check(assign, var_table.emit_ir_to_store(lhs_sym, rhs_value), "storing rhs value");
                    } else {
                        auto const bin_op = assign->op.substr(0, assign->op.size()-1);
                        // Load lhs value
                        // Process operators.(if compound operator, use binary_operator process)
                        auto const lhs_value =
                            tmp_builtin_bin_op_ir_emitter{
                                builder,
                                var_table.emit_ir_to_load(lhs_sym),
                                rhs_value,
                                bin_op
                            }.emit(type::type_of(lhs_expr));
                        check(assign, var_table.emit_ir_to_store(lhs_sym, lhs_value), "storing rhs value in compound assignment '" + bin_op + '\'');
                    }
                } else if (auto const maybe_index_access = get_as<ast::node::index_access>(lhs_expr)) {
                    throw not_implemented_error{assign, __FILE__, __func__, __LINE__, "index access in assignment"};
                } else {
                    DACHS_RAISE_INTERNAL_COMPILATION_ERROR
                }
            }
            , assign->assignees, rhs_values
        );
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
    auto &the_module = *detail::llvm_ir_emitter{llvm::getGlobalContext(), a.name}.emit(a.root);
    std::string errmsg;
    if (llvm::verifyModule(the_module, llvm::ReturnStatusAction, &errmsg)) {
        helper::colorizer<std::string> c;
        std::cout << c.red(errmsg) << std::endl;
    }
    return the_module;
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
