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
#include "dachs/codegen/llvmir/variable_table.hpp"
#include "dachs/codegen/llvmir/ir_builder_helper.hpp"
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
 * class code_generator<ast::node::inu> : code_generator_base { ... };
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
 *     context ctx;
 *     llvm::IRBuilder<> builder;
 *
 * public:
 *
 *     template<class T, class W>
 *     void visit(std::shared_ptr<T> const& n, W const& walker)
 *     {
 *         code_generator<T> generator{n, ctx, builder};
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
    context &ctx;
    variable_table var_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    builtin_function_emitter builtin_func_emitter;
    std::string const& file;
    std::stack<llvm::BasicBlock *> loop_stack; // Loop stack for continue and break statements
    type_ir_emitter type_emitter;

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
    void error(Node const& n, boost::format const& msg) const
    {
        error(n, msg.str());
    }

    template<class Node, class String>
    void error(Node const& n, String const& msg) const
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (boost::format("In line:%1%:col:%2%, %3%") % n->line % n->col % msg).str()
            };
    }

    template<class Node, class T>
    T check(Node const& n, T const v, boost::format const& fmt) const
    {
        return check(n, v, fmt.str());
    }

    template<class Node, class T, class String>
    T check(Node const& n, T const v, String const& feature_name) const
    {
        if (!v) {
            error(n, std::string{"Failed to emit "} + feature_name);
        }
        return v;
    }

    template<class Node, class String, class Value>
    void check_all(Node const& n, String const& feature, Value const v) const
    {
        if (!v) {
            error(n, std::string{"Failed to emit "} + feature);
        }
    }

    template<class Node, class String, class Value, class... Values>
    void check_all(Node const& n, String const& feature, Value const v, Values const... vs) const
    {
        if (!v) {
            error(n, std::string{"Failed to emit "} + feature);
        }
        check_all(n, feature, vs...);
    }

    template<class Node>
    auto get_ir_helper(std::shared_ptr<Node> const& node) noexcept
        -> basic_ir_builder_helper<Node>
    {
        return {node, ctx.builder, ctx.llvm_context};
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
            param_type_irs.push_back(type_emitter.emit(param_sym->type));
        }

        auto func_type_ir = llvm::FunctionType::get(
                type_emitter.emit(*func_def->ret_type),
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

    bool is_available_type_for_binary_expression(type::type const& lhs, type::type const& rhs) const noexcept
    {
        if (!lhs.is_builtin() || !rhs.is_builtin()) {
            return false;
        }

        auto const lhs_builtin_type = *type::get<type::builtin_type>(lhs);
        auto const rhs_builtin_type = *type::get<type::builtin_type>(rhs);
        auto const is_supported = [](auto const& t){ return t->name == "int" || t->name == "float" || t->name == "uint" || t->name == "bool"; };

        return is_supported(lhs_builtin_type) && is_supported(rhs_builtin_type);
    }

public:

    llvm_ir_emitter(std::string const& f, context &c)
        : ctx(c)
        , var_table(ctx)
        , builtin_func_emitter(ctx.llvm_context)
        , file(f)
        , type_emitter(ctx.llvm_context)
    {}

    val emit(ast::node::primary_literal const& pl)
    {
        struct literal_visitor : public boost::static_visitor<val> {
            context &c;
            ast::node::primary_literal const& pl;

            literal_visitor(context &c, ast::node::primary_literal const& pl)
                : c(c), pl(pl)
            {}

            val operator()(char const ch)
            {
                return llvm::ConstantInt::get(
                        emit_type_ir(pl->type, c.llvm_context),
                        static_cast<std::uint8_t const>(ch), false
                    );
            }

            val operator()(double const d)
            {
                return llvm::ConstantFP::get(c.llvm_context, llvm::APFloat(d));
            }

            val operator()(bool const b)
            {
                return b ? llvm::ConstantInt::getTrue(c.llvm_context) : llvm::ConstantInt::getFalse(c.llvm_context);
            }

            val operator()(std::string const& s)
            {
                return c.builder.CreateGlobalStringPtr(s.c_str());
            }

            val operator()(int const i)
            {
                return llvm::ConstantInt::get(
                        emit_type_ir(pl->type, c.llvm_context),
                        static_cast<std::int64_t const>(i), false
                    );
            }

            val operator()(unsigned int const ui)
            {
                return llvm::ConstantInt::get(
                        emit_type_ir(pl->type, c.llvm_context),
                        static_cast<std::uint64_t const>(ui), true
                    );
            }

        } visitor{ctx, pl};

        return check(pl, boost::apply_visitor(visitor, pl->value), "constant");
    }

    val emit(ast::node::symbol_literal const& sym)
    {
        return check(sym, ctx.builder.CreateGlobalStringPtr(sym->value.c_str()), "symbol constant");
    }

    llvm::AllocaInst *emit(ast::node::tuple_literal const& tuple)
    {
        auto const the_type = type::get<type::tuple_type>(tuple->type);
        assert(the_type);

        auto alloca_inst = ctx.builder.CreateAlloca(type_emitter.emit(*the_type));
        auto const& elem_exprs = tuple->element_exprs;
        for (unsigned int idx = 0; idx < elem_exprs.size(); ++idx) {
            auto *const elem_val = emit(elem_exprs[idx]);
            val const indices[] = {
                    ctx.builder.getInt32(0),
                    ctx.builder.getInt32(idx)
                };
            ctx.builder.CreateStore(
                    elem_val,
                    llvm::GetElementPtrInst::Create(
                        alloca_inst,
                        indices,
                        "",
                        ctx.builder.GetInsertBlock()
                    )
                );
        }

        return check(tuple, alloca_inst, "tuple literal");
    }

    llvm::Module *emit(ast::node::inu const& p)
    {
        module = new llvm::Module(file, ctx.llvm_context);
        if (!module) {
            error(p, "module");
        }
        module->setDataLayout(ctx.data_layout->getStringRepresentation());

        builtin_func_emitter.set_module(module);

        // Note:
        // emit Function prototypes in advance for forward reference
        for (auto const& i : p->definitions) {
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

        for (auto const& i : p->definitions) {
            emit(i);
        }

        assert(loop_stack.empty());

        return module;
    }

    void emit(ast::node::parameter const& param)
    {
        assert(!param->param_symbol.expired());

        auto const param_sym = param->param_symbol.lock();
        if (!param_sym->immutable) {
            // Note:
            // The parameter is already registered as register value for variable table in func_prototype()
            // So at first we delete the value and re-register it as allocated value
            auto const register_val = var_table.lookup_register_value(param_sym);
            assert(register_val);
            var_table.erase_register_value(param_sym);

            auto const inst
                = check(
                    param,
                    ctx.builder.CreateAlloca(type_emitter.emit(param_sym->type)),
                    "allocation for variable parameter"
                );

            auto const result = var_table.insert(param_sym, inst);
            assert(result);
            (void) result;
            check(param, ctx.builder.CreateStore(register_val, inst), "storing variable parameter");
        }

    }

    // Note:
    // IR for the function prototype is already emitd in emit(ast::node::inu const&)
    void emit(ast::node::function_definition const& func_def)
    {
        if (func_def->is_template()) {
            for (auto const& i : func_def->instantiated) {
                emit(i);
            }
            return;
        }

        // Note: Already checked the scope is not empty
        auto maybe_prototype_ir = lookup_func(func_def->scope.lock());
        assert(maybe_prototype_ir);
        auto &prototype_ir = *maybe_prototype_ir;
        auto const block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", prototype_ir);
        ctx.builder.SetInsertPoint(block);

        for (auto const& p : func_def->params) {
            emit(p);
        }

        emit(func_def->body);

        if (ctx.builder.GetInsertBlock()->getTerminator()) {
            return;
        }

        if (!func_def->ret_type
                || func_def->kind == ast::symbol::func_kind::proc
                || *func_def->ret_type == type::get_unit_type()) {
            ctx.builder.CreateRetVoid();
        } else {
            // Note:
            // Believe that the insert block is the last block of the function
            ctx.builder.CreateUnreachable();
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
        auto helper = get_ir_helper(if_);

        auto *then_block = helper.create_block_for_parent("if.then");
        auto *else_block = helper.create_block_for_parent("if.else");
        auto *const end_block = helper.create_block("if.end");

        // IR for if-then clause
        val cond_val = emit(if_->condition);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if_stmt_unless"), "unless statement");
        }
        helper.create_cond_br(cond_val, then_block, else_block);
        emit(if_->then_stmts);
        helper.terminate_with_br(end_block, else_block);

        // IR for elseif clause
        for (auto const& elseif : if_->elseif_stmts_list) {
            cond_val = emit(elseif.first);
            then_block = helper.create_block_for_parent("if.then");
            else_block = helper.create_block_for_parent("if.else");
            helper.create_cond_br(cond_val, then_block, else_block);
            emit(elseif.second);
            helper.terminate_with_br(end_block, else_block);
        }

        // IR for else clause
        if (if_->maybe_else_stmts) {
            emit(*if_->maybe_else_stmts);
        }
        helper.terminate_with_br(end_block);

        helper.append_block(end_block);
    }

    void emit(ast::node::return_stmt const& return_)
    {
        if (ctx.builder.GetInsertBlock()->getTerminator()) {
            // Note:
            // Basic block is already terminated.
            // Unreachable return statements should be checked in semantic checks
            return;
        }

        if (return_->ret_exprs.size() == 1) {
            ctx.builder.CreateRet(emit(return_->ret_exprs[0]));
        } else if (return_->ret_exprs.empty()) {
            // TODO:
            // Return statements with no expression in functions should returns unit
            ctx.builder.CreateRetVoid();
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
            return ctx.builder.CreateCall(callee, args);
        }

        auto *const callee = module->getFunction(scope->to_string());

        // TODO
        // Monad invocation

        return ctx.builder.CreateCall(callee, args);
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
            tmp_builtin_unary_op_ir_emitter{ctx.builder, emit(unary->expr), unary->op}.emit(builtin),
            boost::format("unary operator '%1%' (operand's type is '%2%')")
                % unary->op
                % builtin->to_string()
        );
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
            tmp_builtin_bin_op_ir_emitter{ctx.builder, emit(bin_expr->lhs), emit(bin_expr->rhs), bin_expr->op}.emit(lhs_type),
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

    val emit(ast::node::index_access const& access)
    {
        auto const child_type = type::type_of(access->child);
        auto child_val = emit(access->child);

        if (auto const child_tuple_type = type::get<type::tuple_type>(child_type)) {

            // Note:
            // Do not emit index expression because it is a integer literal and it is
            // processed in compile time
            auto const primary_lit = get_as<ast::node::primary_literal>(access->index_expr);
            assert(primary_lit);
            auto const& literal = (*primary_lit)->value;

            auto const emit_index_access
                = [&, this](unsigned int const idx)
                {
                    val const indices[]
                        = {
                            ctx.builder.getInt32(0u),
                            ctx.builder.getInt32(idx)
                        };
                    return ctx.builder.CreateLoad(
                            llvm::GetElementPtrInst::Create(
                                child_val,
                                indices,
                                "",
                                ctx.builder.GetInsertBlock()
                            )
                        );
                };

            if (auto const opt_idx = get_as<int>(literal)) {
                return emit_index_access(static_cast<unsigned int>(*opt_idx));
            } else if (auto const opt_idx = get_as<unsigned int>(literal)) {
                return emit_index_access(*opt_idx);
            } else {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }
        } else {
            return nullptr;
        }
    }

    void emit(ast::node::while_stmt const& while_)
    {
        auto helper = get_ir_helper(while_);

        auto *const cond_block = helper.create_block_for_parent("while.cond");
        auto *const body_block = helper.create_block_for_parent("while.body");
        auto *const exit_block = helper.create_block_for_parent("while.exit");

        // Loop header
        helper.create_br(cond_block);
        val cond_val = emit(while_->condition);
        helper.create_cond_br(cond_val, body_block, exit_block);

        // Loop body
        auto const auto_popper = push_loop(cond_block);
        emit(while_->body_stmts);
        helper.terminate_with_br(cond_block, exit_block);
    }

    void emit(ast::node::initialize_stmt const& init)
    {
        auto const initializee_size = init->var_decls.size();
        auto const initializer_size
            = init->maybe_rhs_exprs ?
                init->maybe_rhs_exprs->size() : 0;

        assert(initializee_size != 0);
        assert(initializer_size != 0);

        auto const emit_alloca_from_sym
            = [&](auto const& sym)
            {
                auto const t = sym->type;
                assert(t);
                auto const inst = check(
                        init,
                        ctx.builder.CreateAlloca(type_emitter.emit(t), nullptr/*array size*/, sym->name),
                        "variable allocation"
                    );

                var_table.insert(std::move(sym), inst);

                return inst;
            };

        if (initializee_size == initializer_size) {
            helper::each(
                    [&, this](auto const& d, auto const& e)
                    {
                        assert(!d->symbol.expired());

                        auto *val = emit(e);
                        auto const sym = d->symbol.lock();
                        if (!d->is_var) {
                            // If the variable is immutable, do not copy rhs value
                            val->setName(sym->name);
                            var_table.insert(std::move(sym), val);
                        } else {
                            if (llvm::isa<llvm::AllocaInst>(val)) {
                                val = check(init, ctx.builder.CreateLoad(val), "loading rhs");
                            }
                            auto *const inst = emit_alloca_from_sym(sym);
                            check(init, ctx.builder.CreateStore(val, inst), "storing to lhs");
                        }
                    }
                    , init->var_decls, *init->maybe_rhs_exprs
                );
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
                                ctx.builder,
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

    void emit(ast::node::case_stmt const& case_)
    {
        auto helper = get_ir_helper(case_);
        auto *const end_block = helper.create_block("case.end");

        llvm::BasicBlock *else_block;
        for (auto const& when_stmts : case_->when_stmts_list) {
            auto *const cond_val = emit(when_stmts.first);
            auto *const when_block = helper.create_block_for_parent("case.when");
            else_block = helper.create_block_for_parent("case.else");

            helper.create_cond_br(cond_val, when_block, else_block);
            emit(when_stmts.second);
            helper.terminate_with_br(end_block, else_block);
        }

        if (case_->maybe_else_stmts) {
            emit(*case_->maybe_else_stmts);
        }
        helper.terminate_with_br(end_block);

        helper.append_block(end_block);
    }

    /*
     * - statement
     *   case v
     *   when a, b
     *   else
     *   end
     *
     * - IR
     *   ; emit a
     *   v == a ? br lthen : br l1
     *   l1:
     *   v == b ? br lthen : br l2
     *   l2:
     *   br lelse
     *   lthen:
     *   ; body
     *   br lend
     *   lelse:
     */
    void emit(ast::node::switch_stmt const& switch_)
    {
        auto helper = get_ir_helper(switch_);
        auto *const end_block = helper.create_block("switch.end");

        auto *const target_val = emit(switch_->target_expr);
        auto const target_type = type::type_of(switch_->target_expr);

        // Emit when clause
        llvm::BasicBlock *else_block;
        for (auto const& when_stmt : switch_->when_stmts_list) {
            assert(when_stmt.first.size() > 0);
            auto *const then_block = helper.create_block("switch.then");
            else_block = helper.create_block("switch.else");

            // Emit condition IRs
            for (auto const& cmp_expr : when_stmt.first) {
                auto *const next_cond_block = helper.create_block("switch.cond.next");

                if (!is_available_type_for_binary_expression(target_type, type::type_of(cmp_expr))) {
                    error(switch_, "Case statement condition now only supports some builtin types");
                }

                auto *const cond_val 
                    = check(
                        switch_,
                        tmp_builtin_bin_op_ir_emitter{
                            ctx.builder,
                            target_val,
                            emit(cmp_expr),
                            "=="
                        }.emit(target_type),
                        "condition in switch statement"
                    );

                helper.create_cond_br(cond_val, then_block, next_cond_block, nullptr);
                helper.append_block(next_cond_block);
            }
            helper.create_br(else_block, nullptr);

            // Note:
            // Though it is easy to insert IR for then block before condition blocks,
            // it is less readable than the IR order implemented here.
            helper.append_block(then_block);
            emit(when_stmt.second);
            helper.terminate_with_br(end_block);
            helper.append_block(else_block);
        }

        if (switch_->maybe_else_stmts) {
            emit(*switch_->maybe_else_stmts);
        }
        helper.terminate_with_br(end_block);

        helper.append_block(end_block);
    }

    val emit(ast::node::if_expr const& if_)
    {
        auto helper = get_ir_helper(if_);

        auto *const then_block = helper.create_block_for_parent("expr.if.then");
        auto *const else_block = helper.create_block_for_parent("expr.if.else");
        auto *const merge_block = helper.create_block_for_parent("expr.if.merge");

        val cond_val = emit(if_->condition_expr);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if_expr_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, else_block);

        auto *const then_val = emit(if_->then_expr);
        helper.terminate_with_br(merge_block, else_block);

        auto *const else_val = emit(if_->else_expr);
        helper.terminate_with_br(merge_block, merge_block);

        auto *const phi = ctx.builder.CreatePHI(type_emitter.emit(if_->type), 2, "expr.if.tmp");
        phi->addIncoming(then_val, then_block);
        phi->addIncoming(else_val, else_block);
        return phi;
    }

    void emit(ast::node::postfix_if_stmt const& postfix_if)
    {
        auto helper = get_ir_helper(postfix_if);

        auto *const then_block = helper.create_block_for_parent("postfixif.then");
        auto *const end_block = helper.create_block_for_parent("postfixif.end");

        val cond_val = emit(postfix_if->condition);
        if (postfix_if->kind == ast::symbol::if_kind::unless) {
            cond_val = check(postfix_if, ctx.builder.CreateNot(cond_val, "postfix_if_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, end_block);

        emit(postfix_if->body);
        helper.terminate_with_br(end_block, end_block);
    }

    val emit(ast::node::cast_expr const& cast)
    {
        auto *const child_val = emit(cast->child);
        auto const child_type = type::type_of(cast->child);
        if (cast->type == child_type) {
            return child_val;
        }

        auto const cast_error
            = [&]
            {
                error(
                    cast,
                    boost::format("Cannot cast from '%1%' to '%2%'\n"
                                  "Note: Now only some built-in primary types are supported."
                                  "(int, uint, float and char)")
                        % type::to_string(child_type)
                        % type::to_string(cast->type)
                );
            };

        auto const maybe_builtin_from_type = type::get<type::builtin_type>(child_type);
        auto const maybe_builtin_to_type = type::get<type::builtin_type>(cast->type);
        if (!maybe_builtin_from_type || !maybe_builtin_to_type) {
            cast_error();
        }

        auto const& from_type = *maybe_builtin_from_type;
        auto const& to_type = *maybe_builtin_to_type;
        auto const& from = from_type->name;
        auto const& to = to_type->name;
        auto *const to_type_ir = type_emitter.emit(to_type);

        auto const cast_check
            = [&](auto const v)
            {
                return check(cast, v, "cast from " + from + " to " + to);
            };

        if (from == "int") {
            if (to == "uint") {
                return child_val; // Note: Do nothing.
            } else if (to == "float") {
                return cast_check(ctx.builder.CreateSIToFP(child_val, to_type_ir));
            } else if (to == "char") {
                return cast_check(ctx.builder.CreateTrunc(child_val, to_type_ir));
            }
        } else if (from == "uint") {
            if (to == "int") {
                return child_val; // Note: Do nothing
            } else if (to == "float") {
                return cast_check(ctx.builder.CreateUIToFP(child_val, to_type_ir));
            } else if (to == "char") {
                return cast_check(ctx.builder.CreateTrunc(child_val, to_type_ir));
            }
        } else if (from == "float") {
            if (to == "int" || to == "char") {
                return cast_check(ctx.builder.CreateFPToSI(child_val, to_type_ir));
            } else if (to == "uint") {
                return cast_check(ctx.builder.CreateFPToUI(child_val, to_type_ir));
            }
        } else if (from == "char") {
            if (to == "int" || to == "uint") {
                return cast_check(ctx.builder.CreateSExt(child_val, to_type_ir));
            } else if (to == "float") {
                return cast_check(ctx.builder.CreateSIToFP(child_val, to_type_ir));
            }
        }

        cast_error();
        return nullptr; // Note: Required to avoid compiler warning.
    }

    template<class T>
    val emit(std::shared_ptr<T> const& node)
    {
        throw not_implemented_error{node, __FILE__, __func__, __LINE__, node->to_string()};
    }
};

} // namespace detail

llvm::Module &emit_llvm_ir(ast::ast const& a, scope::scope_tree const&, context &ctx)
{
    auto &the_module = *detail::llvm_ir_emitter{a.name, ctx}.emit(a.root);
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
