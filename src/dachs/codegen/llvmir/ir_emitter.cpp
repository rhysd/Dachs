#include <unordered_map>
#include <memory>
#include <utility>
#include <string>
#include <vector>
#include <iostream>
#include <stack>
#include <array>
#include <cstdint>
#include <cassert>

#include <boost/format.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/range/irange.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
# include <llvm/Analysis/Verifier.h>
#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
# include <llvm/IR/Verifier.h>
# include <llvm/Support/raw_ostream.h>
#else
# error LLVM: Not supported version.
#endif

#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/each.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/llvm.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/codegen/llvmir/tmp_builtin_operator_ir_emitter.hpp"
#include "dachs/codegen/llvmir/builtin_func_ir_emitter.hpp"
#include "dachs/codegen/llvmir/ir_builder_helper.hpp"
#include "dachs/codegen/llvmir/tmp_member_ir_emitter.hpp"
#include "dachs/codegen/llvmir/tmp_constructor_ir_emitter.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/runtime.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

using helper::variant::apply_lambda;
using helper::variant::get_as;
using helper::dump;
using boost::adaptors::transformed;
using boost::algorithm::all_of;

class llvm_ir_emitter {
    using val = llvm::Value *;
    using self = llvm_ir_emitter;
    using var_table_type = std::unordered_map<symbol::var_symbol, val>;

    llvm::Module *module = nullptr;
    context &ctx;
    semantics::semantics_context const& semantics_ctx;
    var_table_type var_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    std::string const& file;
    std::stack<llvm::BasicBlock *> loop_stack; // Loop stack for continue and break statements
    type_ir_emitter type_emitter;
    builtin_function_emitter builtin_func_emitter;
    tmp_member_ir_emitter member_emitter;
    std::unordered_map<scope::class_scope, llvm::Type *const> class_table;
    builder::alloc_helper alloc_emitter;
    builder::inst_emit_helper inst_emitter;
    tmp_constructor_ir_emitter builtin_ctor_emitter;
    llvm::GlobalVariable *unit_constant = nullptr;

    val lookup_var(symbol::var_symbol const& s) const
    {
        auto const result = var_table.find(s);
        if (result == std::end(var_table)) {
            return nullptr;
        } else {
            return result->second;
        }
    }

    val lookup_var(std::string const& name) const
    {
        auto const result
            = boost::find_if(
                    var_table,
                    [&name](auto const& var){ return var.first->name == name; }
                );

        if (result == boost::end(var_table)) {
            return nullptr;
        } else {
            return result->second;
        }
    }

    bool register_var(symbol::var_symbol const& sym, llvm::Value *const v)
    {
        return var_table.emplace(sym, v).second;
    }

    template<class Symbol>
    bool register_var(Symbol && sym, llvm::Value *const v)
    {
        return var_table.emplace(std::forward<Symbol>(sym), v).second;
    }

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
    [[noreturn]]
    void error(std::shared_ptr<Node> const& n, boost::format const& msg) const
    {
        error(n, msg.str());
    }

    [[noreturn]]
    void error(ast::location_type const& l, boost::format const& msg) const
    {
        error(l, msg.str());
    }

    template<class Node, class String>
    [[noreturn]]
    void error(std::shared_ptr<Node> const& n, String const& msg) const
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (boost::format(" in line:%1%, col:%2%\n  %3%\n") % n->line % n->col % msg).str()
            };
    }

    template<class String>
    [[noreturn]]
    void error(ast::location_type const& l, String const& msg) const
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (
                 boost::format(" in line:%1%, col:%2%\n  %3%\n")
                    % std::get<ast::location::location_index::line>(l)
                    % std::get<ast::location::location_index::col>(l)
                    % msg
                ).str()
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
    auto bb_helper(std::shared_ptr<Node> const& node) noexcept
        -> builder::block_branch_helper<Node>
    {
        return {node, ctx};
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

    template<class FuncValue, class ParamTypes>
    void emit_program_entry_point(FuncValue *const main_func_value, ParamTypes const& param_tys, type::type const& ret_type)
    {
        auto *const entry_func_ty = llvm::FunctionType::get(
                ctx.builder.getInt64Ty(),
                param_tys,
                false
            );

        auto *const entry_func_value = llvm::Function::Create(
                entry_func_ty,
                llvm::Function::ExternalLinkage,
                "main",
                module
            );
        entry_func_value->addFnAttr(llvm::Attribute::NoUnwind);

        auto const entry_block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", entry_func_value);
        ctx.builder.SetInsertPoint(entry_block);

        auto const emit_inner_main_call
            = [&param_tys, main_func_value, entry_func_value, this]
            {
                std::vector<llvm::Value *> call_args;

                if (param_tys.empty()) {
                    return ctx.builder.CreateCall(
                            main_func_value,
                            "main_ret"
                        );
                } else {
                    assert(param_tys.size() == 2u);

                    auto arg_itr = entry_func_value->arg_begin();

                    arg_itr->setName("dachs.entry.argc");
                    call_args.push_back(arg_itr);

                    ++arg_itr;
                    arg_itr->setName("dachs.entry.argv");
                    call_args.push_back(arg_itr);

                    return ctx.builder.CreateCall(
                            main_func_value,
                            std::move(call_args),
                            "main_ret"
                        );
                }
            };

        if (ret_type.is_unit()) {
            emit_inner_main_call();
            ctx.builder.CreateRet(
                    ctx.builder.getInt64(0u)
                );
        } else {
            ctx.builder.CreateRet(
                    emit_inner_main_call()
                );
        }
    }

    void emit_main_func_prototype(ast::node::function_definition const& main_def, scope::func_scope const& main_scope)
    {
        assert(main_scope->params.size() < 2u);

        auto const has_cmdline_arg = main_scope->params.size() == 1u;

        using params_type = std::vector<llvm::Type *>;
        auto const param_tys
            = has_cmdline_arg
                ? params_type{{
                    ctx.builder.getInt32Ty(),
                    llvm::PointerType::getUnqual(
                        ctx.builder.getInt8PtrTy()
                    )
                }}
                : params_type{};

        auto *const func_ty = llvm::FunctionType::get(
                type_emitter.emit(*main_def->ret_type),
                param_tys,
                false
            );

        auto *const func_value = llvm::Function::Create(
                func_ty,
                llvm::Function::ExternalLinkage,
                "dachs.main",
                module
            );

        if (has_cmdline_arg) {
            auto arg_itr = func_value->arg_begin();

            arg_itr->setName("dachs.main.argc");
            // XXX:
            // Owned by variable table only
            register_var(symbol::make<symbol::var_symbol>(nullptr, "dachs.main.argc"), arg_itr);

            ++arg_itr;

            arg_itr->setName(main_scope->params[0]->name);
            register_var(main_scope->params[0], arg_itr);
        }

        func_value->addFnAttr(llvm::Attribute::NoUnwind);
        func_value->addFnAttr(llvm::Attribute::InlineHint);
        func_table.emplace(main_scope, func_value);

        emit_program_entry_point(func_value, param_tys, *main_def->ret_type);
    }

    void emit_func_prototype(ast::node::function_definition const& func_def, scope::func_scope const& scope)
    {
        if (scope->is_main_func()) {
            emit_main_func_prototype(func_def, scope);
            return;
        }

        assert(!scope->is_template());
        std::vector<llvm::Type *> param_type_irs;
        param_type_irs.reserve(func_def->params.size());

        for (auto const& param_sym : scope->params) {
            auto *const t = type_emitter.emit(param_sym->type);
            assert(t);
            param_type_irs.push_back(t);
        }

        auto *const func_type_ir = llvm::FunctionType::get(
                type_emitter.emit(*func_def->ret_type),
                param_type_irs,
                false // Non-variadic
            );

        check(func_def, func_type_ir, "function type");

        // Note:
        // Use to_string() instead of mangling.
        auto *const func_ir = llvm::Function::Create(
                func_type_ir,
                llvm::Function::ExternalLinkage,
                // Note:
                // Simply use "main" because lli requires "main" function as entry point of the program
                scope->to_string(),
                module
            );
        func_ir->addFnAttr(llvm::Attribute::NoUnwind);
        func_ir->addFnAttr(llvm::Attribute::InlineHint);

        check(func_def, func_type_ir, "function");

        {
            auto arg_itr = func_ir->arg_begin();
            auto param_itr = std::begin(scope->params);
            for (; param_itr != std::end(scope->params); ++arg_itr, ++param_itr) {
                arg_itr->setName((*param_itr)->name);
                register_var(*param_itr, arg_itr);
            }
        }

        func_table.emplace(scope, func_ir);
    }

    void emit_func_def_prototype(ast::node::function_definition const& def)
    {
        assert(!def->scope.expired());
        auto const scope = def->scope.lock();
        if (scope->is_template()) {
            for (auto const& instantiated_func_def : def->instantiated) {
                emit_func_def_prototype(instantiated_func_def);
            }
        } else {
            emit_func_prototype(def, scope);
        }
    }

    bool check_builtin_binary_expr_arg_types(type::type const& lhs, type::type const& rhs) const noexcept
    {
        if (type::is_a<type::tuple_type>(lhs) && type::is_a<type::tuple_type>(rhs)) {
            // XXX:
            // Too adhoc.
            // Additional checking is in code generation
            return true;
        }

        if (!lhs.is_builtin() || !rhs.is_builtin()) {
            return false;
        }

        auto const lhs_builtin_type = *type::get<type::builtin_type>(lhs);
        auto const rhs_builtin_type = *type::get<type::builtin_type>(rhs);
        auto const is_supported
            = [](auto const& t)
            {
                    return t->name == "int"
                        || t->name == "float"
                        || t->name == "uint"
                        || t->name == "bool"
                        || t->name == "char"
                        || t->name == "symbol"
                    ;
            };

        return is_supported(lhs_builtin_type) && is_supported(rhs_builtin_type);
    }

    template<class IREmitter>
    struct lhs_of_assign_emitter {

        IREmitter &emitter;
        val const rhs_value;

        void emit_copy_to_lhs(val const lhs_value, type::type const& lhs_type)
        {
            if (!lhs_value) {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }
            assert(lhs_value->getType()->isPointerTy());

            emitter.alloc_emitter.create_deep_copy(
                    rhs_value,
                    emitter.load_aggregate_elem(lhs_value, lhs_type),
                    lhs_type
                );
        }

        void emit(ast::node::var_ref const& ref)
        {
            if (ref->is_ignored_var() && ref->symbol.expired()) {
                // Note:
                // When '_' variable is used at lhs.
                // emit(ast::node::assignment_stmt) already treats this case.
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }

            assert(!ref->symbol.expired());
            auto const sym = ref->symbol.lock();
            emit_copy_to_lhs(emitter.lookup_var(sym), sym->type);
        }

        void emit(ast::node::index_access const& access)
        {
            assert(access->is_assign);

            auto const child_val = emitter.emit(access->child);
            auto const index_val = emitter.deref(emitter.emit(access->index_expr), type::type_of(access->index_expr));
            auto const child_type = type::type_of(access->child);

            if (type::is_a<type::tuple_type>(child_type)) {
                auto const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index_val);
                if (!constant_index) {
                    emitter.error(access, "Index is not a constant.");
                }
                emit_copy_to_lhs(
                        emitter.ctx.builder.CreateStructGEP(
                            child_val,
                            constant_index->getZExtValue()
                        ),
                        access->type
                    );
            } else if (type::is_a<type::array_type>(child_type)) {
                assert(!index_val->getType()->isPointerTy());
                emit_copy_to_lhs(
                    emitter.ctx.builder.CreateInBoundsGEP(
                        child_val,
                        index_val
                    ),
                    access->type
                );
            } else if (!access->callee_scope.expired()) {
                auto const callee = access->callee_scope.lock();
                assert(callee->name == "[]=");

                emitter.check(
                    access,
                    emitter.ctx.builder.CreateCall3(
                        emitter.emit_non_builtin_callee(access, callee),
                        emitter.deref(child_val, child_type),
                        index_val,
                        rhs_value
                    ),
                    "Invalid function call '[]=' for lhs of assignment"
                );
            } else {
                // Note: An exception is thrown
                emitter.error(access, "Invalid assignment to indexed access");
            }
        }

        void emit(ast::node::ufcs_invocation const& ufcs)
        {
            auto const child_type = type::type_of(ufcs->child);

            // Note:
            // When the UFCS invocation is generated for lambda capture access
            if (auto const g = type::get<type::generic_func_type>(child_type)) {
                if (helper::exists(emitter.semantics_ctx.lambda_captures, *g)) {
                    emit_copy_to_lhs(
                        emitter.emit_lambda_capture_access(*g, ufcs),
                        ufcs->type
                    );
                    return;
                }
            }

            assert(ufcs->is_assign);

            auto const receiver_type = type::get<type::class_type>(child_type);
            assert(receiver_type);

            assert(!(*receiver_type)->ref.expired());
            auto const emitted = emitter.emit_instance_var_access((*receiver_type)->ref.lock(), ufcs);

            assert(emitted);
            emit_copy_to_lhs(
                *emitted,
                ufcs->type
            );
        }

        template<class... Args>
        void emit(boost::variant<Args...> const& v)
        {
            apply_lambda([this](auto const& n){ return emit(n); }, v);
        }

        void emit(ast::node::typed_expr const& typed)
        {
            emit(typed->child_expr);
        }

        template<class T>
        void emit(T const&)
        {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    };

    bool treats_by_value(type::type const& t) const
    {
        return t.is_builtin();
    }

    // Note:
    // load_if_ref() strips reference if the value is treated by reference.
    // To determine the value is treated by reference, type::type is used.
    llvm::Value *load_if_ref(llvm::Value *const v, type::type const& t)
    {
        if (auto const builtin = type::get<type::builtin_type>(t)) {
            if ((*builtin)->name != "string" && v->getType()->isPointerTy()) {
                assert(!v->getType()->getPointerElementType()->isPointerTy());
                return ctx.builder.CreateLoad(v);
            } else if ((*builtin)->name == "string" && v->getType()->getPointerElementType()->isPointerTy()) {
                // Note:
                // When the type of string is i8**
                return ctx.builder.CreateLoad(v);
            }
        }
        return v;
    }

    // Note:
    // Aggregate elements in aggregates are stored as a pointer to it.
    // e.g.
    //  [{i32, i32}*], {[i32 x 1]*, {i32, i32}*}
    // However, built-in type is embedded in aggregates directly.
    // So it is necessary to check the type is built-in type and, if not, load instruction
    // needs to be emitted.
    llvm::Value *load_aggregate_elem(llvm::Value *const v, type::type const& elem_type)
    {
        if (elem_type.is_builtin()) {
            // Note:
            // When the value is not an aggregate
            return v;
        }

        if (v->getType()->getPointerElementType()->isPointerTy()) {
            // Note:
            // If the value is an aggregate element of aggregate,
            // the value should be a pointer to pointer to aggregate
            // e.g.
            //   {[int]*}* -> [int]**
            return ctx.builder.CreateLoad(v);
        } else {
            return v;
        }
    }

    llvm::Value *deref(llvm::Value *const v, type::type const& t)
    {
        if (t.is_builtin()) {
            return load_if_ref(v, t);
        } else {
            return load_aggregate_elem(v, t);
        }
    }

    template<class Expr>
    llvm::Value *deref(llvm::Value *const v, Expr const& e)
    {
        return deref(v, type::type_of(e));
    }

    template<class Node>
    llvm::Value *load_if_ref(llvm::Value *const v, Node const& hint)
    {
        return load_if_ref(v, type::type_of(hint));
    }

public:

    llvm_ir_emitter(std::string const& f, context &c, semantics::semantics_context const& sc)
        : ctx(c)
        , semantics_ctx(sc)
        , var_table()
        , file(f)
        , type_emitter(ctx.llvm_context, sc.lambda_captures)
        , builtin_func_emitter(ctx, type_emitter)
        , member_emitter(ctx)
        , alloc_emitter(ctx, type_emitter, sc.lambda_captures)
        , inst_emitter(ctx)
        , builtin_ctor_emitter(ctx, type_emitter, alloc_emitter, module)
    {}

    val emit(ast::node::symbol_literal const& sl)
    {
        runtime::cityhash64<std::uint64_t> hash;
        return llvm::ConstantInt::get(
                llvm::Type::getInt64Ty(ctx.llvm_context),
                hash(sl->value.data(), sl->value.size()),
                false /*isSigned*/
            );
    }

    val emit(ast::node::primary_literal const& pl)
    {
        struct literal_visitor : public boost::static_visitor<val> {
            context &c;
            ast::node::primary_literal const& pl;
            type_ir_emitter &t_emitter;

            literal_visitor(context &c, ast::node::primary_literal const& pl, type_ir_emitter &te)
                : c(c), pl(pl), t_emitter(te)
            {}

            val operator()(char const ch)
            {
                return llvm::ConstantInt::get(
                        t_emitter.emit_alloc_type(pl->type),
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
                return llvm::ConstantInt::getSigned(
                        t_emitter.emit_alloc_type(pl->type),
                        static_cast<std::int64_t const>(i)
                    );
            }

            val operator()(unsigned int const ui)
            {
                return llvm::ConstantInt::get(
                        t_emitter.emit_alloc_type(pl->type),
                        static_cast<std::uint64_t const>(ui), false
                    );
            }

        } visitor{ctx, pl, type_emitter};

        return check(pl, boost::apply_visitor(visitor, pl->value), "constant");
    }

    val emit_tuple_constant(type::tuple_type const& t, std::vector<ast::node::any_expr> const& elem_exprs)
    {
        if (elem_exprs.empty()) {
            if (!unit_constant) {
                unit_constant = new llvm::GlobalVariable(*module, llvm::StructType::get(ctx.llvm_context, {}), true, llvm::GlobalValue::PrivateLinkage, llvm::ConstantStruct::getAnon(ctx.llvm_context, {}));
                unit_constant->setUnnamedAddr(true);
                unit_constant->setName("unit");
            }
            return unit_constant;
        }

        std::vector<val> elem_values;
        elem_values.reserve(elem_exprs.size());
        for (auto const& e : elem_exprs) {
            elem_values.push_back(emit(e));
        }

        if (all_of(elem_values, [](auto const v) -> bool { return llvm::isa<llvm::Constant>(v); })) {
            auto *const ty = type_emitter.emit_alloc_type(t);
            assert(ty->isStructTy());
            std::vector<llvm::Constant *> elem_consts;
            for (auto const v : elem_values) {
                auto *const constant = llvm::dyn_cast<llvm::Constant>(v);
                assert(constant);
                elem_consts.push_back(constant);
            }

            auto const constant = new llvm::GlobalVariable(
                        *module,
                        ty,
                        true/*constant*/,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::getAnon(ctx.llvm_context, elem_consts)
                    );
            constant->setUnnamedAddr(true);

            return constant;
        } else {
            // XXX
            auto *const alloca_inst = alloc_emitter.create_alloca(t);

            for (auto const idx : helper::indices(elem_values)) {
                auto const elem_type = type::type_of(elem_exprs[idx]);
                alloc_emitter.create_deep_copy(
                        elem_values[idx],
                        load_aggregate_elem(
                            ctx.builder.CreateStructGEP(alloca_inst, idx),
                            elem_type
                        ),
                        elem_type
                    );
            }

            return alloca_inst;
        }
    }

    val emit_tuple_constant(std::vector<ast::node::any_expr> const& elem_exprs)
    {
        auto const the_type
            = type::make<type::tuple_type>(
                elem_exprs | transformed([](auto const& e){ return type::type_of(e); })
            );

        return emit_tuple_constant(the_type, elem_exprs);
    }

    val emit_array_constant(type::array_type const& t, std::vector<ast::node::any_expr> const& elem_exprs)
    {
        std::vector<val> elem_values;
        elem_values.reserve(elem_exprs.size());
        for (auto const& e : elem_exprs) {
            elem_values.push_back(emit(e));
        }

        if (all_of(elem_values, [](auto const v) -> bool { return llvm::isa<llvm::Constant>(v); })) {
            auto *const array_ty = type_emitter.emit_alloc_fixed_array(t);

            std::vector<llvm::Constant *> elem_consts;
            for (auto const v : elem_values) {
                auto *const constant = llvm::dyn_cast<llvm::Constant>(v);
                assert(constant);
                elem_consts.push_back(constant);
            }

            auto const constant = new llvm::GlobalVariable(
                        *module,
                        array_ty,
                        true /*constant*/,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantArray::get(array_ty, elem_consts)
                    );
            constant->setUnnamedAddr(true);

            return ctx.builder.CreateConstInBoundsGEP2_32(constant, 0u, 0u);
        } else {
            auto *const alloca_inst = alloc_emitter.create_alloca(t, false/*not initialize*/, "arraylit");
            for (auto const idx : helper::indices(elem_exprs)) {
                alloc_emitter.create_deep_copy(
                        elem_values[idx],
                        load_aggregate_elem(
                            ctx.builder.CreateConstInBoundsGEP1_32(alloca_inst, idx),
                            t->element_type
                        ),
                        t->element_type
                    );
            }

            return alloca_inst;
        }
    }

    val emit(ast::node::tuple_literal const& tuple)
    {
        assert(type::is_a<type::tuple_type>(tuple->type));
        return check(
                tuple,
                emit_tuple_constant(
                    *type::get<type::tuple_type>(tuple->type),
                    tuple->element_exprs
                ),
                "tuple literal"
            );
    }

    val emit(ast::node::array_literal const& array)
    {
        assert(type::is_a<type::array_type>(array->type));
        return check(
                array,
                emit_array_constant(
                    *type::get<type::array_type>(array->type),
                    array->element_exprs
                ),
                "array literal"
            );
    }

    val emit(ast::node::lambda_expr const& lambda)
    {
        return emit(lambda->receiver);
    }

    llvm::Module *emit(ast::node::inu const& p)
    {
        module = new llvm::Module(file, ctx.llvm_context);
        if (!module) {
            error(p, "module");
        }

        module->setDataLayout(ctx.data_layout->getStringRepresentation());
        module->setTargetTriple(ctx.triple.getTriple());

        builtin_func_emitter.set_module(module);

        // Note:
        // emit Function prototypes in advance for forward reference
        for (auto const& f : p->functions) {
            emit_func_def_prototype(f);
        }

        // Note:
        // Then, emit other functions
        auto const emit_defs
            = [this](auto const& defs)
            {
                for (auto const& d : defs) {
                    emit(d);
                }
            };

        emit_defs(p->global_constants);
        emit_defs(p->functions);

        assert(loop_stack.empty());

        return module;
    }

    void emit(ast::node::parameter const& param)
    {
        if (param->name == "_") {
            return;
        }

        assert(!param->param_symbol.expired());
        auto const param_sym = param->param_symbol.lock();
        if (param_sym->immutable
         || param->is_receiver
         || param_sym->is_instance_var()) {
            return;
        }

        // Note:
        // Mutable parameters are copied not to affect the original value

        // Note:
        // The parameter is already registered as register value for variable table in emit_func_prototype()
        // So at first we delete the value and re-register it as allocated value
        auto const param_val = lookup_var(param_sym);
        var_table.erase(param_sym);

        assert(type_emitter.emit(param_sym->type) == param_val->getType());

        auto const inst
            = check(
                param,
                alloc_emitter.alloc_and_deep_copy(param_val, param_sym->type, param->name),
                "allocation for variable parameter"
            );

        auto const result = register_var(param_sym, inst);
        assert(result);
        (void) result;
    }

    void emit_instance_var_init_params(scope::func_scope const& ctor)
    {
        assert(ctor->is_member_func);

        auto const& self_sym = ctor->params[0];
        assert(type::is_a<type::class_type>(self_sym->type));
        auto const receiver_type = *type::get<type::class_type>(self_sym->type);
        assert(!receiver_type->ref.expired());
        auto const clazz = receiver_type->ref.lock();

        auto *const self_val = lookup_var(self_sym);
        assert(self_val);

        for (auto const& p : ctor->params) {
            if (p->is_instance_var()) {
                auto *const initializer_val = lookup_var(p);
                assert(initializer_val);

                auto const offset = clazz->get_instance_var_offset_of(p->name.substr(1u)/* omit '@' */);
                assert(offset);
                auto *const dest_val = load_aggregate_elem(
                        ctx.builder.CreateStructGEP(self_val, *offset),
                        p->type
                    );
                assert(dest_val);
                alloc_emitter.create_deep_copy(initializer_val, dest_val, p->type);
            }
        }
    }

    // Note:
    // IR for the function prototype is already emitd in emit(ast::node::inu const&)
    void emit(ast::node::function_definition const& func_def)
    {
        auto const scope = func_def->scope.lock();
        if (scope->is_template()) {
            for (auto const& i : func_def->instantiated) {
                emit(i);
            }
            return;
        }

        // Note: Already checked the scope is not empty
        auto maybe_prototype_ir = lookup_func(scope);
        assert(maybe_prototype_ir);
        auto &prototype_ir = *maybe_prototype_ir;
        auto const block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", prototype_ir);
        ctx.builder.SetInsertPoint(block);

        for (auto const& p : func_def->params) {
            emit(p);
        }

        if (scope->is_ctor()) {
            emit_instance_var_init_params(scope);
        }

        emit(func_def->body);

        if (ctx.builder.GetInsertBlock()->getTerminator()) {
            return;
        }

        if (!func_def->ret_type
                || func_def->kind == ast::symbol::func_kind::proc
                || *func_def->ret_type == type::get_unit_type()) {
            ctx.builder.CreateRet(
                emit_tuple_constant(type::get_unit_type(), {})
            );
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
            if (ctx.builder.GetInsertBlock()->getTerminator()) {
                // Note:
                // If current basic block is already terminated,
                // no more statement should be emitted.
                return;
            }
            emit(stmt);
        }
    }

    void emit(ast::node::if_stmt const& if_)
    {
        auto helper = bb_helper(if_);

        auto *then_block = helper.create_block_for_parent("if.then");
        auto *else_block = helper.create_block_for_parent("if.else");
        auto *const end_block = helper.create_block("if.end");

        // IR for if-then clause
        val cond_val = load_if_ref(emit(if_->condition), if_->condition);
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
            ctx.builder.CreateRet(load_if_ref(emit(return_->ret_exprs[0]), type::type_of(return_->ret_exprs[0])));
        } else {
            assert(type::is_a<type::tuple_type>(return_->ret_type));
            ctx.builder.CreateRet(
                load_if_ref(
                    emit_tuple_constant(
                        *type::get<type::tuple_type>(return_->ret_type),
                        return_->ret_exprs
                    ),
                    return_->ret_type
                )
            );
        }
    }

    template<class Node, class Scope>
    llvm::Function *emit_non_builtin_callee(Node const& n, Scope const& scope)
    {
        if (scope->is_template()) {
            // XXX:
            // It seems internal compilation error now
            error(n, boost::format("'%1%' is unresolved overloaded function '%2%'") % scope->name % scope->to_string());
        }

        return check(
                n,
                module->getFunction(scope->to_string()),
                boost::format("generic function variable '%1%' for '%2%'") % scope->to_string() % scope->type.to_string()
            );
    }

    template<class Node, class Scope, class Exprs>
    llvm::Function *emit_callee(Node const& n, Scope const& scope, Exprs const& args)
    {
        if (scope->is_builtin) {
            std::vector<type::type> param_types;
            param_types.reserve(scope->params.size());
            for (auto const& e : args) {
                param_types.push_back(type::type_of(e));
            }

            return check(
                    n,
                    builtin_func_emitter.emit(scope->name, param_types),
                    boost::format("builtin function '%1%'") % scope->to_string()
                );
        }

        return emit_non_builtin_callee(n, scope);
    }

    val emit(ast::node::func_invocation const& invocation)
    {
        std::vector<val> args;
        args.reserve(invocation->args.size());
        for (auto const& a : invocation->args) {
            args.push_back(load_if_ref(emit(a), a));
        }

        auto const child_type = type::type_of(invocation->child);
        auto const generic = type::get<type::generic_func_type>(child_type);
        if (!generic) {
            // TODO:
            // Deal with func_type
            error(invocation, boost::format("calls '%1%' type variable which is not callable") % child_type.to_string());
        }

        assert(!invocation->callee_scope.expired());
        assert((*generic)->ref && !(*generic)->ref->expired());
        auto const callee = invocation->callee_scope.lock();

        // Note:
        // Add a receiver for lambda function invocation
        if (callee->is_anonymous()) {
            args.insert(std::begin(args), load_if_ref(emit(invocation->child), child_type));
        } else {
            emit(invocation->child);
        }

        return check(
                    invocation,
                    ctx.builder.CreateCall(
                        emit_callee(invocation, callee, invocation->args),
                        args
                    ),
                    "invalid function call"
                );
    }

    val emit(ast::node::unary_expr const& unary)
    {
        auto const operand_type = type::type_of(unary->expr);
        auto *const operand_value = load_if_ref(emit(unary->expr), operand_type);

        if (auto const b = type::get<type::builtin_type>(operand_type)) {
            auto const& builtin = *b;
            auto const& name = builtin->name;

            if (name != "int" && name != "float" && name != "bool" && name != "uint") {
                error(unary, "Unary expression now only supports float, int, bool and uint");
            }

            return check(
                unary,
                tmp_builtin_unary_op_ir_emitter{ctx, operand_value, unary->op}.emit(builtin),
                boost::format("unary operator '%1%' (operand's type is '%2%')")
                    % unary->op
                    % builtin->to_string()
            );
        }

        if (unary->callee_scope.expired()) {
            error(
                unary,
                boost::format("Invalid unary expression is found.  No operator function '%1%' for type '%2%'")
                    % unary->op % operand_type.to_string()
            );
        }

        auto const callee = unary->callee_scope.lock();
        assert(!callee->is_anonymous());
        assert(!callee->is_builtin);

        return check(
                unary,
                ctx.builder.CreateCall(
                    emit_non_builtin_callee(unary, callee),
                    operand_value
                ),
                "invalid unary expression function call"
            );

    }

    template<class Node>
    val emit_binary_expr(
            Node const& node,
            std::string const& op,
            type::type const& lhs_type,
            type::type const& rhs_type,
            val const lhs_value,
            val const rhs_value,
            scope::weak_func_scope const& callee_scope
    ) {
        if (check_builtin_binary_expr_arg_types(lhs_type, rhs_type)) {
            return check(
                node,
                tmp_builtin_bin_op_ir_emitter{
                    ctx,
                    lhs_value,
                    rhs_value,
                    op
                }.emit(lhs_type),
                boost::format("binary operator '%1%' (lhs type is '%2%', rhs type is '%3%')")
                    % op
                    % type::to_string(lhs_type)
                    % type::to_string(rhs_type)
            );
        }

        if (callee_scope.expired()) {
            error(
                node,
                boost::format("Invalid binary expression is found.  No operator function '%1%' for lhs type '%2%' and rhs type '%3%'")
                    % op % lhs_type.to_string() % rhs_type.to_string()
            );
        }

        auto const callee = callee_scope.lock();
        assert(!callee->is_anonymous());
        assert(!callee->is_builtin);

        return check(
                node,
                ctx.builder.CreateCall2(
                    emit_non_builtin_callee(node, callee),
                    lhs_value,
                    rhs_value
                ),
                "invalid binary expression function call"
            );
    }

    val emit(ast::node::binary_expr const& bin_expr)
    {
        auto const lhs_type = type::type_of(bin_expr->lhs);
        auto const rhs_type = type::type_of(bin_expr->rhs);

        return emit_binary_expr(
                bin_expr,
                bin_expr->op,
                rhs_type,
                lhs_type,
                load_if_ref(emit(bin_expr->lhs), lhs_type),
                load_if_ref(emit(bin_expr->rhs), rhs_type),
                bin_expr->callee_scope
            );
    }

    val emit(ast::node::var_ref const& var)
    {
        assert(!var->symbol.expired());
        auto *const looked_up = lookup_var(var->symbol.lock());
        if (looked_up) {
            return looked_up;
        }

        if (auto const g = type::get<type::generic_func_type>(var->type)) {
            // Note:
            // Reach here when the variable is function variable and it is not a lambda object.
            // This is because a lambda object has already been emitted as a variable and lookup_var()
            // returns the corresponding llvm::Value.
            assert(llvm::dyn_cast<llvm::StructType>(type_emitter.emit_alloc_type(*g))->getNumElements() == 0u);
            return emit_tuple_constant(type::get_unit_type(), {});
        } else {
            error(var, boost::format("Invalid variable reference '%1%'. Its type is '%2%'") % var->name % var->type.to_string());
        }
    }

    val emit(ast::node::index_access const& access)
    {
        if (!access->callee_scope.expired()) {
            auto const callee = access->callee_scope.lock();
            assert(!callee->is_anonymous());
            assert(!callee->is_builtin);

            return check(
                    access,
                    ctx.builder.CreateCall2(
                        emit_non_builtin_callee(access, callee),
                        load_if_ref(emit(access->child), access->child),
                        load_if_ref(emit(access->index_expr), access->index_expr)
                    ),
                    "user-defined index access operator"
                );
        }

        auto const result = inst_emitter.emit_builtin_element_access(
                emit(access->child),
                load_if_ref(emit(access->index_expr), access->index_expr),
                type::type_of(access->child)
            );

        if (auto const err = get_as<std::string>(result)) {
            error(access, *err);
        }

        auto const ret = get_as<llvm::Value *>(result);
        assert(ret);
        return *ret;
    }

    val emit_lambda_capture_access(type::generic_func_type const& lambda, ast::node::ufcs_invocation const& ufcs)
    {
        auto const& indexed_by_introduced = semantics_ctx.lambda_captures.at(lambda).get<semantics::tags::introduced>();
        auto const capture = indexed_by_introduced.find(ufcs);
        auto *const child_val = emit(ufcs->child);

        if (capture == boost::end(indexed_by_introduced)) {
            // Note:
            // If the capture map exists but no capture is found,
            // it is non-captured lambda.
            return emit_tuple_constant(type::get_unit_type(), {});
        }

        return load_aggregate_elem(
                ctx.builder.CreateStructGEP(child_val, capture->offset),
                ufcs->type
            );
    }

    boost::optional<val> emit_instance_var_access(scope::class_scope const& scope, ast::node::ufcs_invocation const& ufcs)
    {
        auto *const child_val = load_if_ref(emit(ufcs->child), ufcs->child);

        auto const offset_of
            = [&scope](auto const& name)
                -> boost::optional<unsigned>
            {
                auto const& syms = scope->instance_var_symbols;
                for (unsigned i = 0; i < syms.size(); ++i) {
                    if (name == syms[i]->name) {
                        return i;
                    }
                }
                return boost::none;
            };

        // TODO:
        // Now the parameter type of class is not a pointer but itself.  It will be pointer because Dachs has reference semantics.

        if (auto const maybe_idx = offset_of(ufcs->member_name)) {
            auto const& idx = *maybe_idx;

            return load_aggregate_elem(
                    ctx.builder.CreateStructGEP(child_val, idx),
                    ufcs->type
                );
        }

        return boost::none;
    }

    val emit_data_member(ast::node::ufcs_invocation const& ufcs)
    {
        auto const child_type = type::type_of(ufcs->child);

        // Note:
        // When the UFCS invocation is generated for lambda capture access
        if (auto const g = type::get<type::generic_func_type>(child_type)) {
            if (helper::exists(semantics_ctx.lambda_captures, *g)) {
                return emit_lambda_capture_access(*g, ufcs);
            }
        }

        // Note:
        // When accessing the data member.
        // Now, built-in data member is only available.
        if (auto const clazz = type::get<type::class_type>(child_type)) {
            if (auto const v = emit_instance_var_access((*clazz)->ref.lock(), ufcs)) {
                return *v;
            }
        }

        // TODO:
        // Change type of 'args' which is a parameter for main.
        // Now it is 'static_array(string)' but it should be array(string).
        // The change will remove workarounds like below.
        if (auto const a = type::get<type::array_type>(child_type)) {
            if (ufcs->member_name == "size" && (*a)->element_type.is_builtin("string") && !(*a)->size ) {
                return check(
                        ufcs,
                        ctx.builder.CreateIntCast(
                            lookup_var("dachs.main.argc"),
                            ctx.builder.getInt64Ty(),
                            true
                        ),
                        "access to 'dachs.main.argc'"
                    );
            }
        }

        // Note:
        // When the argument is 'argv', which is an argument of 'main' function
        auto *const child_value = emit(ufcs->child);

        // Note:
        // Do not use get_operand() because GEP is emitted
        // in member_emitter internally.
        return check(
                ufcs,
                member_emitter.emit_builtin_instance_var(
                    child_value,
                    ufcs->member_name,
                    child_type
                ),
                "data member access"
            );
    }

    val emit(ast::node::ufcs_invocation const& ufcs)
    {
        if (ufcs->is_instance_var_access()) {
            return emit_data_member(ufcs);
        }

        assert(!ufcs->callee_scope.expired());

        std::vector<val> args = {
                load_if_ref(emit(ufcs->child), ufcs->child)
            };
        auto const callee = ufcs->callee_scope.lock();

        return check(
                    ufcs,
                    ctx.builder.CreateCall(
                        emit_callee(ufcs, callee, std::vector<ast::node::any_expr>{{ufcs->child}}),
                        args
                    ),
                    "UFCS function invocation"
                );
    }

    void emit(ast::node::while_stmt const& while_)
    {
        auto helper = bb_helper(while_);

        auto *const cond_block = helper.create_block_for_parent("while.cond");
        auto *const body_block = helper.create_block_for_parent("while.body");
        auto *const exit_block = helper.create_block_for_parent("while.exit");

        // Loop header
        helper.create_br(cond_block);
        val cond_val = emit(while_->condition);
        helper.create_cond_br(load_if_ref(cond_val, while_->condition), body_block, exit_block);

        // Loop body
        auto const auto_popper = push_loop(cond_block);
        emit(while_->body_stmts);
        helper.terminate_with_br(cond_block, exit_block);
    }

    void emit(ast::node::for_stmt const& for_)
    {
        auto helper = bb_helper(for_);

        // Note:
        // Now array is only supported

        val range_val = load_aggregate_elem(
                emit(for_->range_expr),
                type::type_of(for_->range_expr)
            );

        auto const range_type = type::type_of(for_->range_expr);
        auto const array_range_type = type::get<type::array_type>(range_type);
        auto const class_range_type = type::get<type::class_type>(range_type);

        assert(
                (
                    array_range_type &&
                    (*array_range_type)->size
                ) || (
                    class_range_type &&
                    !for_->index_callee_scope.expired() &&
                    !for_->size_callee_scope.expired()
                )
            );

        val const range_size_val
            = array_range_type
                ? static_cast<val>(
                        ctx.builder.getInt64(
                            *(*array_range_type)->size
                        )
                    )
                : check(
                        for_,
                        ctx.builder.CreateCall(
                            emit_non_builtin_callee(for_, for_->size_callee_scope.lock()),
                            range_val
                        ),
                        "size() function call for user defined container"
                    );

        auto *const counter_val = ctx.builder.CreateAlloca(ctx.builder.getInt64Ty(), nullptr, "for.i");
        ctx.builder.CreateStore(ctx.builder.getInt64(0u), counter_val);

        if (for_->iter_vars.size() != 1u) {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        auto const& param = for_->iter_vars[0];
        auto const sym = param->param_symbol;
        auto *const allocated =
            param->is_var
                ? alloc_emitter.create_alloca(param->type, false /*zero init?*/, param->name)
                : nullptr;

        // Note:
        // Do not emit parameter by emit(ast::node::parameter const&)

        auto *const header_block = helper.create_block_for_parent("for.header");
        auto *const body_block = helper.create_block_for_parent("for.body");
        auto *const footer_block = helper.create_block_for_parent("for.footer");

        helper.create_br(header_block);

        auto *const loaded_counter_val = ctx.builder.CreateLoad(counter_val, "for.i.loaded");
        helper.create_cond_br(
                ctx.builder.CreateICmpULT(loaded_counter_val, range_size_val),
                body_block,
                footer_block
            );

        // TODO:
        // Make for's variable definition as assignment statement?

        if (param->name != "_" && !sym.expired()) {
            val const elem_ptr_val =
                array_range_type
                    ? ctx.builder.CreateInBoundsGEP(
                            range_val,
                            (val [2]){
                                ctx.builder.getInt64(0u),
                                loaded_counter_val
                            },
                            param->name
                        )
                    : check(
                            for_,
                            ctx.builder.CreateCall2(
                                emit_non_builtin_callee(for_, for_->index_callee_scope.lock()),
                                range_val,
                                loaded_counter_val
                            ),
                            "index access call for 'for' statement"
                        )
                ;

            auto const s = sym.lock();

            if (allocated) {
                alloc_emitter.create_deep_copy(elem_ptr_val, allocated, s->type);
                register_var(s, allocated);
            } else {
                register_var(s, elem_ptr_val);
                elem_ptr_val->setName(param->name);
            }
        }

        emit(for_->body_stmts);

        ctx.builder.CreateStore(ctx.builder.CreateAdd(loaded_counter_val, ctx.builder.getInt64(1u)), counter_val);
        helper.create_br(header_block, footer_block);
    }

    void emit(ast::node::initialize_stmt const& init)
    {
        if (!init->maybe_rhs_exprs) {
            // TODO:
            // Emit default construction

            // Note:
            // Variable declaration without definition is not initialized
            for (auto const& d : init->var_decls) {
                auto const sym = d->symbol.lock();
                assert(d->maybe_type);
                auto *const allocated = alloc_emitter.create_alloca(sym->type);
                assert(allocated);
                register_var(std::move(sym), allocated);
            }
            return;
        }

        auto const& rhs_exprs = *init->maybe_rhs_exprs;
        auto const initializee_size = init->var_decls.size();
        auto const initializer_size = rhs_exprs.size();

        assert(initializee_size != 0);
        assert(initializer_size != 0);

        auto const initialize
            = [&, this](auto const& decl, auto *const value)
            {
                if (decl->name == "_" && decl->symbol.expired()) {
                    return;
                }

                assert(!decl->symbol.expired());

                auto const sym = decl->symbol.lock();
                auto const& type = sym->type;
                if (!decl->self_symbol.expired()) {
                    assert(decl->is_instance_var());

                    // Note:
                    // When the instance variable declaration in constructor,
                    // it should initialize the instance variable of 'self' object.

                    auto const self_sym = decl->self_symbol.lock();
                    assert(type::is_a<type::class_type>(self_sym->type));
                    auto const receiver_type = *type::get<type::class_type>(self_sym->type);
                    assert(!receiver_type->ref.expired());
                    auto const clazz = receiver_type->ref.lock();

                    auto const offset
                        = clazz->get_instance_var_offset_of(decl->name.substr(1u));
                    assert(offset);

                    auto const self_val = lookup_var(self_sym);
                    assert(self_val);
                    assert(self_val->getType()->isPointerTy());

                    auto *const dest_val = load_aggregate_elem(ctx.builder.CreateStructGEP(self_val, *offset), type);
                    assert(dest_val);

                    alloc_emitter.create_deep_copy(value, dest_val, type);

                    dest_val->setName(decl->name);

                } else if (decl->is_var) {
                    auto *const allocated = alloc_emitter.alloc_and_deep_copy(value, type, sym->name);
                    assert(allocated);
                    register_var(std::move(sym), allocated);
                } else {
                    // If the variable is immutable, do not copy rhs value
                    register_var(std::move(sym), value);
                }
            };

        if (initializee_size == initializer_size) {
            helper::each(
                    [&, this](auto const& d, auto const& e)
                    {
                        initialize(d, emit(e));
                    }
                    , init->var_decls, rhs_exprs
                );
        } else if (initializee_size == 1) {
            assert(initializer_size > 1);

            auto *const rhs_tuple_value
                = emit_tuple_constant(rhs_exprs);

            initialize(init->var_decls[0], rhs_tuple_value);
        } else if (initializer_size == 1) {
            assert(initializee_size > 1);
            auto const& rhs_expr = (rhs_exprs)[0];
            auto *const rhs_value = emit(rhs_expr);
            auto *const rhs_type = rhs_value->getType();

            std::vector<val> rhs_values;

            // Note:
            // If the rhs type is a pointer, it means that rhs is allocated value
            // and I should use GEP to get the element of it.

            assert(rhs_type->isPointerTy() && rhs_type->getPointerElementType()->isStructTy());

            auto *const rhs_struct_type = llvm::dyn_cast<llvm::StructType>(rhs_type->getPointerElementType());
            assert(rhs_struct_type);
            for (auto const idx : helper::indices(rhs_struct_type->getNumElements())) {
                rhs_values.push_back(ctx.builder.CreateLoad(ctx.builder.CreateStructGEP(rhs_value, idx)));
            }

            helper::each(initialize , init->var_decls, rhs_values);
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    void emit(ast::node::assignment_stmt const& assign)
    {
        assert(assign->op == "=");
        assert(assign->assignees.size() == assign->rhs_exprs.size());

        helper::each(
            [&, this](auto const& lhs_expr, auto const& rhs_expr)
            {
                auto const lhs_type = type::type_of(lhs_expr);
                if (!lhs_type) {
                    // Note: When lhs is '_'
                    return;
                }

                auto const rhs_type = type::type_of(rhs_expr);

                auto *const rhs_value = load_if_ref(emit(rhs_expr), rhs_type);
                lhs_of_assign_emitter<self>{*this, rhs_value}.emit(lhs_expr);
            }
            , assign->assignees, assign->rhs_exprs
        );
    }

    void emit(ast::node::case_stmt const& case_)
    {
        auto helper = bb_helper(case_);
        auto *const end_block = helper.create_block("case.end");

        llvm::BasicBlock *else_block;
        for (auto const& when_stmts : case_->when_stmts_list) {
            auto *const cond_val = load_if_ref(emit(when_stmts.first), when_stmts.first);
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
        auto helper = bb_helper(switch_);
        auto *const end_block = helper.create_block("switch.end");

        auto *const target_val = load_if_ref(emit(switch_->target_expr), switch_->target_expr);
        auto const target_type = type::type_of(switch_->target_expr);

        // Emit when clause
        llvm::BasicBlock *else_block;
        helper::each(
            [&, this](auto const& when_stmt, auto const& callees) {
                assert(when_stmt.first.size() > 0);
                auto *const then_block = helper.create_block("switch.then");
                else_block = helper.create_block("switch.else");

                // Emit condition IRs
                helper::each(
                    [&, this](auto const& cmp_expr, auto const& callee) {
                        auto *const next_cond_block = helper.create_block("switch.cond.next");
                        auto const cmp_type = type::type_of(cmp_expr);

                        auto *const compared_val
                            = emit_binary_expr(
                                    ast::node::location_of(cmp_expr),
                                    "==",
                                    target_type,
                                    cmp_type,
                                    target_val,
                                    load_if_ref(emit(cmp_expr), cmp_type),
                                    callee
                                );

                        helper.create_cond_br(compared_val, then_block, next_cond_block, nullptr);
                        helper.append_block(next_cond_block);
                    }
                    , when_stmt.first, callees
                );
                helper.create_br(else_block, nullptr);

                // Note:
                // Though it is easy to insert IR for then block before condition blocks,
                // it is less readable than the IR order implemented here.
                helper.append_block(then_block);
                emit(when_stmt.second);
                helper.terminate_with_br(end_block);
                helper.append_block(else_block);
            }
            , switch_->when_stmts_list, switch_->when_callee_scopes
        );

        if (switch_->maybe_else_stmts) {
            emit(*switch_->maybe_else_stmts);
        }
        helper.terminate_with_br(end_block);

        helper.append_block(end_block);
    }

    val emit(ast::node::if_expr const& if_)
    {
        auto helper = bb_helper(if_);

        auto *const then_block = helper.create_block_for_parent("expr.if.then");
        auto *const else_block = helper.create_block_for_parent("expr.if.else");
        auto *const merge_block = helper.create_block_for_parent("expr.if.merge");

        val cond_val = load_if_ref(emit(if_->condition_expr), if_->condition_expr);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if_expr_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, else_block);

        auto *const then_val = load_if_ref(emit(if_->then_expr), if_->then_expr);
        helper.terminate_with_br(merge_block, else_block);

        auto *const else_val = load_if_ref(emit(if_->else_expr), if_->else_expr);
        helper.terminate_with_br(merge_block, merge_block);

        auto *const phi = ctx.builder.CreatePHI(type_emitter.emit(if_->type), 2, "expr.if.tmp");
        phi->addIncoming(then_val, then_block);
        phi->addIncoming(else_val, else_block);
        return phi;
    }

    val emit(ast::node::typed_expr const& typed)
    {
        return emit(typed->child_expr);
    }

    void emit(ast::node::postfix_if_stmt const& postfix_if)
    {
        auto helper = bb_helper(postfix_if);

        auto *const then_block = helper.create_block_for_parent("postfixif.then");
        auto *const end_block = helper.create_block_for_parent("postfixif.end");

        val cond_val = load_if_ref(emit(postfix_if->condition), postfix_if->condition);
        if (postfix_if->kind == ast::symbol::if_kind::unless) {
            cond_val = check(postfix_if, ctx.builder.CreateNot(cond_val, "postfix_if_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, end_block);

        emit(postfix_if->body);
        helper.terminate_with_br(end_block, end_block);
    }

    val emit(ast::node::cast_expr const& cast)
    {
        auto const child_type = type::type_of(cast->child);
        auto *const child_val = load_if_ref(emit(cast->child), child_type);
        if (cast->type == child_type) {
            return child_val;
        }

        auto const cast_error
            = [&]
            {
                error(
                    cast,
                    boost::format("Cannot cast from '%1%' to '%2%'\n"
                                  "  Note: Now only some built-in primary types are supported."
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
        } else if (from == "string" && to == "symbol") {
            auto *const cityhash = builtin_func_emitter.emit_cityhash_func();
            return ctx.builder.CreateCall(
                    cityhash,
                    child_val
                );
        }

        cast_error();
        return nullptr; // Note: Required to avoid compiler warning.
    }

    val emit_class_object_construct(ast::node::object_construct const& obj, type::class_type const& t, std::vector<val> && arg_values)
    {
        auto const obj_val = alloc_emitter.create_alloca(t);

        arg_values.insert(std::begin(arg_values), obj_val);

        assert(!obj->callee_ctor_scope.expired());
        auto const ctor_scope = obj->callee_ctor_scope.lock();

        ctx.builder.CreateCall(
                emit_non_builtin_callee(obj, ctor_scope),
                std::move(arg_values)
            );

        return obj_val;
    }

    val emit(ast::node::object_construct const& obj)
    {
        std::vector<val> arg_vals;
        for (auto const& e : obj->args) {
            arg_vals.push_back(load_if_ref(emit(e), e));
        }

        if (auto const c = type::get<type::class_type>(obj->type)) {
            return emit_class_object_construct(obj, *c, std::move(arg_vals));
        }

        return check(obj, builtin_ctor_emitter.emit(obj->type, arg_vals), "object construction");
    }

    template<class T>
    val emit(std::shared_ptr<T> const& node)
    {
        throw not_implemented_error{node, __FILE__, __func__, __LINE__, "In LLVM code generation: " + node->to_string()};
    }
};

} // namespace detail

llvm::Module &emit_llvm_ir(ast::ast const& a, semantics::semantics_context const& sctx, context &ctx)
{
    auto &the_module = *detail::llvm_ir_emitter{a.name, ctx, sctx}.emit(a.root);
    std::string errmsg;

#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
    if (llvm::verifyModule(the_module, llvm::ReturnStatusAction, &errmsg)) {

#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
    llvm::raw_string_ostream rso(errmsg);
    if (llvm::verifyModule(the_module, &rso)) {

#else
# error LLVM: Not supported version.
#endif

        helper::colorizer c;
        std::cerr << c.red(errmsg) << std::endl;
        // DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }

    return the_module;
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
