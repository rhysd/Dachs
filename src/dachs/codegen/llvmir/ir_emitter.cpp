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

#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/codegen/llvmir/tmp_builtin_operator_ir_emitter.hpp"
#include "dachs/codegen/llvmir/builtin_func_ir_emitter.hpp"
#include "dachs/codegen/llvmir/variable_table.hpp"
#include "dachs/codegen/llvmir/ir_builder_helper.hpp"
#include "dachs/codegen/llvmir/tmp_member_ir_emitter.hpp"
#include "dachs/codegen/llvmir/tmp_constructor_ir_emitter.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/runtime/runtime.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/each.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/llvm.hpp"

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

    llvm::Module *module = nullptr;
    context &ctx;
    semantics::semantics_context const& semantics_ctx;
    variable_table var_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    builtin_function_emitter builtin_func_emitter;
    std::string const& file;
    std::stack<llvm::BasicBlock *> loop_stack; // Loop stack for continue and break statements
    type_ir_emitter type_emitter;
    tmp_member_ir_emitter member_emitter;
    tmp_constructor_ir_emitter builtin_ctor_emitter;
    std::unordered_map<scope::class_scope, llvm::Type *const> class_table;
    builder::alloc_helper allocator;

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
    void error(Node const& n, boost::format const& msg) const
    {
        error(n, msg.str());
    }

    template<class Node, class String>
    [[noreturn]]
    void error(Node const& n, String const& msg) const
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (boost::format(" in line:%1%:col:%2%\n  %3%\n") % n->line % n->col % msg).str()
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

    void emit_main_func_prototype(ast::node::function_definition const& main_def, scope::func_scope const& main_scope)
    {
        assert(main_scope->params.size() < 2u);

        auto const has_cmdline_arg = main_scope->params.size() == 1u;

        using params_type = std::vector<llvm::Type *>;
        auto param_tys
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
                "main",
                module
            );

        if (has_cmdline_arg) {
            auto arg_itr = func_value->arg_begin();

            arg_itr->setName("dachs.main.argc");
            // XXX:
            // Owned by variable table only
            var_table.insert(symbol::make<symbol::var_symbol>(nullptr, "dachs.main.argc"), arg_itr);

            ++arg_itr;

            arg_itr->setName(main_scope->params[0]->name);
            var_table.insert(main_scope->params[0], arg_itr);
        }

        func_value->addFnAttr(llvm::Attribute::NoUnwind);
        func_table.emplace(main_scope, func_value);
    }

    void emit_func_prototype(ast::node::function_definition const& func_def, scope::func_scope const& scope)
    {
        if (scope->name == "main") {
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

    bool is_available_type_for_binary_expression(type::type const& lhs, type::type const& rhs) const noexcept
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

        explicit lhs_of_assign_emitter(IREmitter &e)
            : emitter(e)
        {}

        val emit(ast::node::var_ref const& ref)
        {
            if (ref->is_ignored_var() && ref->symbol.expired()) {
                // Note:
                // Ignore '_' variable
                return nullptr;
            }

            assert(!ref->symbol.expired());
            return emitter.var_table.lookup_value(ref->symbol.lock());
        }

        val emit(ast::node::index_access const& access)
        {
            auto const child_val = emitter.emit(access->child);
            auto const index_val = emitter.get_operand(emitter.emit(access->index_expr));
            auto const child_type = type::type_of(access->child);
            if (type::is_a<type::tuple_type>(child_type)) {
                auto const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index_val);
                if (!constant_index) {
                    emitter.error(access, "Index is not a constant.");
                }
                return emitter.ctx.builder.CreateStructGEP(child_val, constant_index->getZExtValue());
            } else if (type::is_a<type::array_type>(child_type)) {
                assert(!index_val->getType()->isPointerTy());
                return emitter.ctx.builder.CreateInBoundsGEP(
                        child_val,
                        (val [2]){
                            emitter.ctx.builder.getInt64(0u),
                            index_val
                        }
                    );
            } else {
                // Note: An exception is thrown
                emitter.error(access, "Not a tuple value (in assignment statement)");
            }
        }

        val emit(ast::node::ufcs_invocation const& ufcs)
        {
            auto const receiver_type = type::get<type::class_type>(type::type_of(ufcs->child));
            if (!receiver_type) {
                return nullptr;
            }

            auto const receiver_val = emitter.emit(ufcs->child);
            assert(receiver_val->getType()->isPointerTy() && receiver_val->getType()->getPointerElementType()->isStructTy());

            assert(!(*receiver_type)->ref.expired());
            auto const clazz = (*receiver_type)->ref.lock();
            auto const offset = clazz->get_instance_var_offset_of(ufcs->member_name);
            assert(offset);

            return emitter.ctx.builder.CreateStructGEP(receiver_val, *offset);
        }

        template<class... Args>
        val emit(boost::variant<Args...> const& v)
        {
            return apply_lambda([this](auto const& n){ return emit(n); }, v);
        }

        val emit(ast::node::typed_expr const& typed)
        {
            return emit(typed->child_expr);
        }

        template<class T>
        val emit(T const&)
        {
            return nullptr;
        }
    };

public:

    llvm_ir_emitter(std::string const& f, context &c, semantics::semantics_context const& sc)
        : ctx(c)
        , semantics_ctx(sc)
        , var_table(ctx)
        , builtin_func_emitter(ctx.llvm_context)
        , file(f)
        , type_emitter(ctx.llvm_context, sc.lambda_captures)
        , member_emitter(ctx)
        , builtin_ctor_emitter(ctx, type_emitter)
        , allocator(ctx)
    {}

    // Note:
    // get_operand() strips pointer if needed.
    // Operation instructions and call operation requires a value as operand.
    // The value must be loaded by load instruction if the value is allocated pointer
    // by alloca instruction or GEP pointing to the element of tuple or array.
    template<class T>
    val get_operand(T *const value) const
    {
        // XXX:
        // Too hacky.
        // User defined class is treated with a pointer to its resource.
        auto const *const type = value->getType();
        if (type->isPointerTy()) {
            if (auto const *const struct_ty = llvm::dyn_cast<llvm::StructType>(type->getPointerElementType())) {
                if (struct_ty->hasName()) {
                    return value;
                }
            }
        }

        // XXX:
        // This condition is too ad hoc.
        if (llvm::isa<llvm::AllocaInst>(value) || llvm::isa<llvm::GetElementPtrInst>(value)) {
            return ctx.builder.CreateLoad(value);
        } else {
            return value;
        }
    }

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
                        t_emitter.emit(pl->type),
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
                        t_emitter.emit(pl->type),
                        static_cast<std::int64_t const>(i)
                    );
            }

            val operator()(unsigned int const ui)
            {
                return llvm::ConstantInt::get(
                        t_emitter.emit(pl->type),
                        static_cast<std::uint64_t const>(ui), false
                    );
            }

        } visitor{ctx, pl, type_emitter};

        return check(pl, boost::apply_visitor(visitor, pl->value), "constant");
    }

    template<class Expr>
    val emit_tuple_constant(type::tuple_type const& t, std::vector<Expr> const& elem_exprs)
    {
        std::vector<val> elem_values;
        elem_values.reserve(elem_exprs.size());
        for (auto const& e : elem_exprs) {
            elem_values.push_back(emit(e));
        }

        if (all_of(elem_values, [](auto const v) -> bool { return llvm::isa<llvm::Constant>(v); })) {
            std::vector<llvm::Constant *> elem_consts;
            for (auto const v : elem_values) {
                auto *const constant = llvm::dyn_cast<llvm::Constant>(v);
                assert(constant);
                elem_consts.push_back(constant);
            }

            return llvm::ConstantStruct::getAnon(ctx.llvm_context, elem_consts);
        } else {
            auto *const alloca_inst = ctx.builder.CreateAlloca(type_emitter.emit(t));
            for (auto const idx : helper::indices(elem_values.size())) {
                auto *const elem_val = get_operand(elem_values[idx]);
                ctx.builder.CreateStore(
                        elem_val,
                        // Note:
                        // CreateStructGEP is also available for array value because
                        // it is equivalent to CreateConstInBoundsGEP2_32(v, 0u, i).
                        ctx.builder.CreateStructGEP(alloca_inst, idx)
                    );
            }

            return alloca_inst;
        }
    }

    template<class Expr>
    val emit_tuple_constant(std::vector<Expr> const& elem_exprs)
    {
        auto const the_type
            = type::make<type::tuple_type>(
                elem_exprs | transformed([](auto const& e){ return type::type_of(e); })
            );

        return emit_tuple_constant(the_type, elem_exprs);
    }

    template<class Expr>
    val emit_array_constant(type::array_type const& t, std::vector<Expr> const& elem_exprs)
    {
        std::vector<val> elem_values;
        elem_values.reserve(elem_exprs.size());
        for (auto const& e : elem_exprs) {
            elem_values.push_back(emit(e));
        }

        if (all_of(elem_values, [](auto const v) -> bool { return llvm::isa<llvm::Constant>(v); })) {
            std::vector<llvm::Constant *> elem_consts;
            for (auto const v : elem_values) {
                auto *const constant = llvm::dyn_cast<llvm::Constant>(v);
                assert(constant);
                elem_consts.push_back(constant);
            }

            return llvm::ConstantArray::get(type_emitter.emit_fixed_array(t), elem_consts);
        } else {
            auto *const alloca_inst = ctx.builder.CreateAlloca(type_emitter.emit(t));
            for (auto const idx : helper::indices(elem_exprs.size())) {
                ctx.builder.CreateStore(
                        get_operand(emit(elem_exprs[idx])),
                        // Note:
                        // CreateStructGEP is also available for array value because
                        // it is equivalent to CreateConstInBoundsGEP2_32(v, 0u, i).
                        ctx.builder.CreateStructGEP(alloca_inst, idx)
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
        auto const register_val = var_table.lookup_register_value(param_sym);
        assert(register_val);
        var_table.erase_register_value(param_sym);

        assert(type_emitter.emit(param_sym->type) == register_val->getType());

        auto const inst
            = check(
                param,
                allocator.alloc_and_deep_copy(register_val, param_sym->name),
                "allocation for variable parameter"
            );

        auto const result = var_table.insert(param_sym, inst);
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

        auto *const self_val = var_table.lookup_value(self_sym);
        assert(self_val);

        for (auto const& p : ctor->params) {
            if (p->is_instance_var()) {
                auto *const initializer_val = var_table.lookup_value(p);
                assert(initializer_val);

                auto const offset = clazz->get_instance_var_offset_of(p->name.substr(1u)/* omit '@' */);
                assert(offset);
                auto *const dest_val = ctx.builder.CreateStructGEP(self_val, *offset);
                assert(dest_val);
                allocator.create_deep_copy(initializer_val, dest_val);
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
                llvm::ConstantStruct::getAnon(ctx.llvm_context, {})
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
        val cond_val = get_operand(emit(if_->condition));
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
            ctx.builder.CreateRet(get_operand(emit(return_->ret_exprs[0])));
        } else {
            assert(type::is_a<type::tuple_type>(return_->ret_type));
            ctx.builder.CreateRet(
                get_operand(
                    emit_tuple_constant(
                        *type::get<type::tuple_type>(return_->ret_type),
                        return_->ret_exprs
                    )
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
                    boost::format("builtin function '%1%' can't be emit") % scope->to_string()
                );
        }

        return emit_non_builtin_callee(n, scope);
    }

    val emit(ast::node::func_invocation const& invocation)
    {
        std::vector<val> args;
        args.reserve(invocation->args.size());
        for (auto const& a : invocation->args) {
            args.push_back(get_operand(emit(a)));
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
            args.insert(std::begin(args), get_operand(emit(invocation->child)));
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
            tmp_builtin_unary_op_ir_emitter{ctx, get_operand(emit(unary->expr)), unary->op}.emit(builtin),
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
            error(bin_expr, "Binary expression now supports only some builtin types");
        }

        return check(
            bin_expr,
            tmp_builtin_bin_op_ir_emitter{ctx, get_operand(emit(bin_expr->lhs)), get_operand(emit(bin_expr->rhs)), bin_expr->op}.emit(lhs_type),
            boost::format("binary operator '%1%' (lhs type is '%2%', rhs type is '%3%')")
                % bin_expr->op
                % type::to_string(lhs_type)
                % type::to_string(rhs_type)
        );
    }

    val emit(ast::node::var_ref const& var)
    {
        assert(!var->symbol.expired());
        auto *const looked_up = var_table.lookup_value(var->symbol.lock());
        if (looked_up) {
            return looked_up;
        }

        if (auto const g = type::get<type::generic_func_type>(var->type)) {
            return llvm::ConstantStruct::get(type_emitter.emit(*g), {});
        } else {
            error(var, boost::format("Invalid variable reference '%1%'. Its type is '%2%'") % var->name % var->type.to_string());
        }
    }

    val emit(ast::node::index_access const& access)
    {
        auto const child_type = type::type_of(access->child);
        auto *const child_val = emit(access->child);
        auto *const index_val = get_operand(emit(access->index_expr));
        auto *const ty = child_val->getType();
        auto *const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index_val);
        auto const is_ptr = ty->isPointerTy();

        auto const with_check
            = [&, this](auto *const v)
            {
                return check(access, v, "index access");
            };

        if (type::is_a<type::tuple_type>(child_type)) {

            // Note:
            // Do not emit index expression because it is a integer literal and it is
            // processed in compile time
            if (!constant_index) {
                error(access, "Index is not a constant.");
            }

            assert(ty->isStructTy() || (is_ptr && ty->getPointerElementType()->isStructTy()));

            return with_check(
                    ty->isStructTy() ?
                        ctx.builder.CreateExtractValue(child_val, constant_index->getZExtValue()) :
                        ctx.builder.CreateStructGEP(child_val, constant_index->getZExtValue())
                );

        } else if (auto const maybe_array_type = type::get<type::array_type>(child_type)) {
            assert(index_val->getType()->isIntegerTy());

            if (constant_index && !is_ptr) {
                auto const idx = constant_index->getZExtValue();
                assert(ty->isArrayTy());
                auto const size = ty->getArrayNumElements();

                if (idx >= size) {
                    error(access, boost::format("Array index is out of bounds (size:%1%, index:%2%)") % size % idx);
                }

                return with_check(ctx.builder.CreateExtractValue(child_val, idx));
            } else {

                auto const emit_i32
                    = [this](auto *const v)
                    {
                        return v->getType()->isIntegerTy(32u)
                                ? v
                                : ctx.builder.CreateIntCast(v, ctx.builder.getInt32Ty(), true);
                    };

                // Note:
                // When i8** (it means the arguments of 'main')
                if (is_ptr
                    && ty->getPointerElementType()->isPointerTy()
                    && ty->getPointerElementType()->getPointerElementType()->isIntegerTy(8u)
                ) {
                    return ctx.builder.CreateGEP(
                            child_val,
                            emit_i32(index_val)
                        );
                }

                return with_check(
                        ctx.builder.CreateInBoundsGEP(
                            ty->isPointerTy() ?
                                child_val :
                                allocator.alloc_and_deep_copy(child_val),
                            (val [2]){
                                ctx.builder.getInt32(0u),
                                emit_i32(index_val)
                            }
                        )
                    );
            }

        } else {
            error(access, "Not a tuple or array value");
        }
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
            return llvm::ConstantStruct::getAnon(ctx.llvm_context, {});
        }

        return child_val->getType()->isStructTy() ?
            ctx.builder.CreateExtractValue(child_val, capture->offset) :
            ctx.builder.CreateStructGEP(child_val, capture->offset);
    }

    boost::optional<val> emit_instance_var_access(scope::class_scope const& scope, ast::node::ufcs_invocation const& ufcs)
    {
        auto *const child_val = get_operand(emit(ufcs->child));

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
            return ctx.builder.CreateStructGEP(child_val, idx);
        }

        return boost::none;
    }

    val emit_data_member(ast::node::ufcs_invocation const& ufcs)
    {

        // Note:
        // When the UFCS invocation is generated for lambda capture access
        if (auto const g = type::get<type::generic_func_type>(type::type_of(ufcs->child))) {
            if (helper::exists(semantics_ctx.lambda_captures, *g)) {
                return emit_lambda_capture_access(*g, ufcs);
            }
        }

        // Note:
        // When accessing the data member.
        // Now, built-in data member is only available.
        auto const child_type = type::type_of(ufcs->child);
        if (auto const clazz = type::get<type::class_type>(child_type)) {
            if (auto const v = emit_instance_var_access((*clazz)->ref.lock(), ufcs)) {
                return *v;
            }
        }

        // Note:
        // When the argument is 'argv', which is an argument of 'main' function
        auto *const child_value = emit(ufcs->child);
        auto *const ty = child_value->getType();
        if (ty->isPointerTy()
            && ty->getPointerElementType()->isPointerTy()
            && ty->getPointerElementType()->getPointerElementType()->isIntegerTy(8u)
        ) {
            // Note:
            // When the type is 'i8**'
            if (ufcs->member_name == "size") {
                return check(
                        ufcs,
                        ctx.builder.CreateIntCast(
                            var_table.lookup_value_by_name("dachs.main.argc"),
                            ctx.builder.getInt64Ty(),
                            true
                        ),
                        "access to 'dachs.main.argc'"
                    );
            }
        }


        // Note:
        // Do not use get_operand() because GEP is emitted
        // in member_emitter internally.
        return check(
                ufcs,
                member_emitter.emit_builtin_instance_var(
                    child_value,
                    ufcs->member_name,
                    type::type_of(ufcs->child)
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

        std::vector<val> args = {get_operand(emit(ufcs->child))};
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
        helper.create_cond_br(get_operand(cond_val), body_block, exit_block);

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

        val range_val = emit(for_->range_expr);
        if (!range_val->getType()->isPointerTy()) {
            if (auto *const a = llvm::dyn_cast<llvm::ConstantArray>(range_val)) {
                range_val = new llvm::GlobalVariable(*module, a->getType(), true, llvm::GlobalVariable::PrivateLinkage, a);
            } else {
                range_val = check(
                        for_,
                        allocator.alloc_and_deep_copy(range_val),
                        "allocation in for statement"
                    );
            }
        }

        assert(range_val->getType()->isPointerTy());
        assert(range_val->getType()->getPointerElementType()->isArrayTy());

        auto *const range_size_val = ctx.builder.getInt32(range_val->getType()->getPointerElementType()->getArrayNumElements());
        auto *const counter_val = ctx.builder.CreateAlloca(ctx.builder.getInt32Ty(), nullptr, "for.i");
        ctx.builder.CreateStore(ctx.builder.getInt32(0u), counter_val);

        if (for_->iter_vars.size() != 1u) {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        auto const& param = for_->iter_vars[0];
        auto const sym = param->param_symbol;
        auto const iter_t = type_emitter.emit(param->type);
        auto *const allocated =
            param->is_var ? ctx.builder.CreateAlloca(iter_t, nullptr, param->name) : nullptr;

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

        if (param->name != "_" || !sym.expired()) {
            auto *const elem_ptr_val =
                ctx.builder.CreateInBoundsGEP(
                    range_val,
                    (val [2]){
                        ctx.builder.getInt32(0u),
                        loaded_counter_val
                    },
                    param->name
                );

            if (allocated) {
                allocator.create_deep_copy(elem_ptr_val, allocated);
                var_table.insert(sym.lock(), allocated);
            } else {
                var_table.insert(sym.lock(), elem_ptr_val);
                elem_ptr_val->setName(param->name);
            }
        }

        emit(for_->body_stmts);

        ctx.builder.CreateStore(ctx.builder.CreateAdd(loaded_counter_val, ctx.builder.getInt32(1u)), counter_val);
        helper.create_br(header_block, footer_block);
    }

    void emit(ast::node::initialize_stmt const& init)
    {
        if (!init->maybe_rhs_exprs) {
            // Variable declaration without definition is not initialized
            for (auto const& d : init->var_decls) {
                auto const sym = d->symbol.lock();
                assert(d->maybe_type);
                auto const type_ir = type_emitter.emit(sym->type);
                auto *const allocated = ctx.builder.CreateAlloca(type_ir, nullptr, sym->name);
                ctx.builder.CreateMemSet(
                        allocated,
                        ctx.builder.getInt8(0u),
                        ctx.data_layout->getTypeAllocSize(type_ir),
                        ctx.data_layout->getPrefTypeAlignment(type_ir)
                    );
                var_table.insert(std::move(sym), allocated);
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

                    auto const self_val = var_table.lookup_value(self_sym);
                    assert(self_val);
                    assert(self_val->getType()->isPointerTy());

                    auto *const dest_val = ctx.builder.CreateStructGEP(self_val, *offset);
                    assert(dest_val);

                    allocator.create_deep_copy(value, dest_val);

                } else if (decl->is_var) {
                    auto *const allocated = allocator.alloc_and_deep_copy(value, sym->name);
                    assert(allocated);
                    var_table.insert(std::move(sym), allocated);
                } else {
                    // If the variable is immutable, do not copy rhs value
                    var_table.insert(std::move(sym), value);
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

            assert(rhs_type->isStructTy() || (rhs_type->isPointerTy() && rhs_type->getPointerElementType()->isStructTy()));

            if (rhs_type->isStructTy()) {
                auto *const rhs_struct_type = llvm::dyn_cast<llvm::StructType>(rhs_type);
                assert(rhs_struct_type);
                for (auto const idx : boost::irange(0u, rhs_struct_type->getNumElements())) {
                    rhs_values.push_back(ctx.builder.CreateExtractValue(rhs_value, idx));
                }
            } else {
                auto *const rhs_struct_type = llvm::dyn_cast<llvm::StructType>(rhs_type->getPointerElementType());
                assert(rhs_struct_type);
                for (auto const idx : boost::irange(0u, rhs_struct_type->getNumElements())) {
                    rhs_values.push_back(ctx.builder.CreateLoad(ctx.builder.CreateStructGEP(rhs_value, idx)));
                }
            }

            helper::each(initialize , init->var_decls, rhs_values);
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
        auto const is_compound_assign = assign->op != "=";
        assert(assignee_size > 0 && assigner_size > 0);

        if (assignee_size == assigner_size) {
            helper::each(
                    [&](auto const& lhs, auto const& rhs)
                    {
                        if (is_compound_assign && !is_available_type_for_binary_expression(type::type_of(lhs), type::type_of(rhs))) {
                            error(assign, "Binary expression now only supports float, int, bool and uint");
                        }
                        rhs_values.push_back(emit(rhs));
                    }, assign->assignees, assign->rhs_exprs);
        } else if (assigner_size == 1) {
            assert(assignee_size > 1);
            auto *const rhs_value = emit(assign->rhs_exprs[0]);
            auto *const rhs_struct_type = llvm::dyn_cast<llvm::StructType>(rhs_value->getType()->getPointerElementType());
            assert(rhs_struct_type);

            for (auto const idx : boost::irange(0u, rhs_struct_type->getNumElements())) {
                rhs_values.push_back(ctx.builder.CreateLoad(ctx.builder.CreateStructGEP(rhs_value, idx)));
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        assert(assignee_size == rhs_values.size());

        auto const assignment_emitter =
            [&, this](auto const& lhs_expr, auto *const rhs_value)
            {
                val value_to_assign = rhs_value;

                auto *const lhs_value =
                    lhs_of_assign_emitter<self>{*this}.emit(lhs_expr);

                if (!lhs_value) {
                    DACHS_RAISE_INTERNAL_COMPILATION_ERROR
                }

                assert(lhs_value->getType()->isPointerTy());

                if (is_compound_assign) {
                    auto const bin_op = assign->op.substr(0, assign->op.size()-1);
                    auto const lhs_type = type::type_of(lhs_expr);
                    value_to_assign =
                        check(
                            assign,
                            tmp_builtin_bin_op_ir_emitter{
                                ctx,
                                ctx.builder.CreateLoad(lhs_value),
                                rhs_value,
                                bin_op
                            }.emit(lhs_type),
                            boost::format("binary expression (operator is '%1%', operand type is '%2%')")
                                % bin_op
                                % type::to_string(lhs_type)
                        );
                }

                allocator.create_deep_copy(value_to_assign, lhs_value);
            };

        helper::each(assignment_emitter, assign->assignees, rhs_values);
    }

    void emit(ast::node::case_stmt const& case_)
    {
        auto helper = bb_helper(case_);
        auto *const end_block = helper.create_block("case.end");

        llvm::BasicBlock *else_block;
        for (auto const& when_stmts : case_->when_stmts_list) {
            auto *const cond_val = get_operand(emit(when_stmts.first));
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

        auto *const target_val = get_operand(emit(switch_->target_expr));
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
                            ctx,
                            target_val,
                            get_operand(emit(cmp_expr)),
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
        auto helper = bb_helper(if_);

        auto *const then_block = helper.create_block_for_parent("expr.if.then");
        auto *const else_block = helper.create_block_for_parent("expr.if.else");
        auto *const merge_block = helper.create_block_for_parent("expr.if.merge");

        val cond_val = get_operand(emit(if_->condition_expr));
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if_expr_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, else_block);

        auto *const then_val = get_operand(emit(if_->then_expr));
        helper.terminate_with_br(merge_block, else_block);

        auto *const else_val = get_operand(emit(if_->else_expr));
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

        val cond_val = get_operand(emit(postfix_if->condition));
        if (postfix_if->kind == ast::symbol::if_kind::unless) {
            cond_val = check(postfix_if, ctx.builder.CreateNot(cond_val, "postfix_if_unless"), "unless expression");
        }
        helper.create_cond_br(cond_val, then_block, end_block);

        emit(postfix_if->body);
        helper.terminate_with_br(end_block, end_block);
    }

    void emit(ast::node::let_stmt const& let)
    {
        for (auto const& i : let->inits) {
            emit(i);
        }
        emit(let->child_stmt);
    }

    val emit(ast::node::cast_expr const& cast)
    {
        auto *const child_val = get_operand(emit(cast->child));
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
        }

        cast_error();
        return nullptr; // Note: Required to avoid compiler warning.
    }

    val emit_class_object_construct(ast::node::object_construct const& obj, type::class_type const& t, std::vector<val> && arg_values)
    {
        auto const type_ir = type_emitter.emit(t)->getPointerElementType();
        assert(type_ir);
        auto const obj_val = ctx.builder.CreateAlloca(type_ir);
        ctx.builder.CreateMemSet(
                obj_val,
                ctx.builder.getInt8(0u),
                ctx.data_layout->getTypeAllocSize(type_ir),
                ctx.data_layout->getPrefTypeAlignment(type_ir)
            );

        arg_values.insert(std::begin(arg_values), get_operand(obj_val));

        assert(!obj->callee_ctor_scope.expired());
        auto const ctor_scope = obj->callee_ctor_scope.lock();

        ctx.builder.CreateCall(
                emit_non_builtin_callee(obj, ctor_scope),
                arg_values
            );

        return obj_val;
    }

    val emit(ast::node::object_construct const& obj)
    {
        std::vector<val> arg_vals;
        for (auto const& e : obj->args) {
            arg_vals.push_back(get_operand(emit(e)));
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
    }

    return the_module;
}

} // namespace llvm
} // namespace codegen
} // namespace dachs
