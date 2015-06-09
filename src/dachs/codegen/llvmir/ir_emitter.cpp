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
#include "dachs/codegen/llvmir/ir_utility.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/codegen/llvmir/gc_alloc_emitter.hpp"
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

struct emit_skipper {};

using helper::variant::apply_lambda;
using helper::variant::get_as;
using helper::dump;
using boost::adaptors::transformed;
using boost::algorithm::all_of;

class llvm_ir_emitter {
    using val = llvm::Value *;
    using self = llvm_ir_emitter;
    using var_table_type = std::unordered_map<symbol::var_symbol, val>;

    template<class Emitter>
    friend class tmp_constructor_ir_emitter;

    llvm::Module *module = nullptr;
    context &ctx;
    semantics::semantics_context const& semantics_ctx;
    var_table_type var_table;
    std::unordered_map<scope::func_scope, llvm::Function *const> func_table;
    std::string const& file;
    std::stack<llvm::BasicBlock *> loop_stack; // Loop stack for continue and break statements
    type_ir_emitter type_emitter;
    gc_alloc_emitter gc_emitter;
    tmp_member_ir_emitter member_emitter;
    std::unordered_map<scope::class_scope, llvm::Type *const> class_table;
    builder::allocation_helper alloc_helper;
    builder::inst_emit_helper inst_emitter;
    builtin_function_emitter builtin_func_emitter;
    tmp_constructor_ir_emitter<llvm_ir_emitter> builtin_ctor_emitter;

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
            automatic_popper(decltype(s) &s, llvm::BasicBlock *const loop) noexcept
                : s(s), pushed_loop(loop)
            {}
            ~automatic_popper() noexcept
            {
                assert(!s.empty());
                assert(s.top() == pushed_loop);
                s.pop();
            }
        } popper{loop_stack, loop_value};
        return popper;
    }

    template<class Node>
    auto bb_helper(std::shared_ptr<Node> const& node) const noexcept
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

    template<class FuncValue>
    void emit_program_entry_point(FuncValue *const main_func_value, bool const has_cmdline_arg, type::type const& ret_type)
    {
        auto *const entry_func_ty = llvm::FunctionType::get(
                ctx.builder.getInt32Ty(),
                (llvm::Type *[2]){
                    ctx.builder.getInt32Ty(),
                    ctx.builder.getInt8PtrTy()->getPointerTo()
                },
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
            = [has_cmdline_arg, main_func_value, entry_func_value, this]
            {
                gc_emitter.emit_init();

                if (!has_cmdline_arg) {
                    return ctx.builder.CreateCall(
                            main_func_value,
                            "main_ret"
                        );
                } else {
                    auto arg_itr = entry_func_value->arg_begin();

                    arg_itr->setName("dachs.entry.argc");
                    auto const arg0 = arg_itr;

                    ++arg_itr;
                    arg_itr->setName("dachs.entry.argv");

                    auto const arg1 = arg_itr;

                    assert(semantics_ctx.main_arg_constructor);
                    auto const& ctor = *semantics_ctx.main_arg_constructor;

                    // Note:
                    // Allocate object to construct of 'argv' class
                    auto const obj_val = alloc_helper.create_alloca(ctor->params[0]->type);
                    obj_val->setName("argv_obj");

                    // Note:
                    // Emit constructor call for 'argv'
                    ctx.builder.CreateCall3(
                            emit_non_builtin_callee(ctor->get_ast_node(), ctor),
                            obj_val,
                            ctx.builder.CreateSExt(arg0, ctx.builder.getInt64Ty()),
                            arg1,
                            "argv"
                        );

                    return ctx.builder.CreateCall(
                            main_func_value,
                            obj_val,
                            "main_ret"
                        );
                }
            };

        if (ret_type.is_unit()) {
            emit_inner_main_call();
            ctx.builder.CreateRet(
                    ctx.builder.getInt32(0u)
                );
        } else {
            ctx.builder.CreateRet(
                    ctx.builder.CreateIntCast(
                        emit_inner_main_call(),
                        ctx.builder.getInt32Ty(),
                        true /*is_signed*/
                    )
                );
        }
    }

    void emit_func_prototype(ast::node::function_definition const& func_def, scope::func_scope const& scope)
    {
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
                !scope->is_main_func() ? scope->to_string() : "dachs.main",
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

    template<class IREmitter, class Node>
    struct lhs_of_assign_emitter {

        IREmitter &emitter;
        val const rhs_value;
        Node const& node;

        void emit_copy_to_lhs(val const lhs_value, type::type const& lhs_type)
        {
            assert(lhs_value);
            assert(lhs_value->getType()->isPointerTy());

            if (auto const copier = emitter.semantics_ctx.copier_of(lhs_type)) {
                auto *const copied
                    = emitter.emit_copier_call(
                            node,
                            rhs_value,
                            *copier
                        );

                // Note:
                // Below assertion never fails because the copy target is class type
                // and it is treated as the pointer value to struct.
                assert(copied->getType() == lhs_value->getType());

                emitter.ctx.builder.CreateStore(
                        emitter.ctx.builder.CreateLoad(copied),
                        lhs_value
                    );
            } else {
                emitter.alloc_helper.create_deep_copy(
                        rhs_value,
                        emitter.load_aggregate_elem(lhs_value, lhs_type),
                        lhs_type
                    );
            }
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
                    error(access, "Index is not a constant.");
                }
                emit_copy_to_lhs(
                        emitter.ctx.builder.CreateStructGEP(
                            child_val,
                            constant_index->getZExtValue(),
                            "tuple.idx"
                        ),
                        access->type
                    );
            } else if (type::is_a<type::array_type>(child_type)) {
                assert(!index_val->getType()->isPointerTy());
                emit_copy_to_lhs(
                    emitter.ctx.builder.CreateInBoundsGEP(
                        child_val,
                        index_val,
                        "array.idx"
                    ),
                    access->type
                );
            } else if (auto const ptr_type = type::get<type::pointer_type>(child_type)) {
                assert(!index_val->getType()->isPointerTy());

                auto *const indexed_ptr_val =
                    emitter.ctx.builder.CreateInBoundsGEP(
                        emitter.load_if_ref(child_val, child_type),
                        index_val,
                        "ptr.idx"
                    );

                if (auto const pointee_class = type::get<type::class_type>((*ptr_type)->pointee_type)) {

                    // Note:
                    // This is a workaround for a problem of memory allocation of pointer type.
                    // If the member is a pointer type value, compiler allocates the member memory becausee
                    // pointer construction does 'shallow' memory allocation.
                    // See issue #71 for more detail.
                    //
                    // Memory allocation for pointer
                    //   [X*] : pointer(X){4u}
                    //   |
                    //   |   -------------------------------------------------
                    //   |-->| sizeof(X) | sizeof(X) | sizeof(X) | sizeof(X) |
                    //       -------------------------------------------------
                    //          |
                    //          | Memory for member pointer is NOT allocated at construction of pointer type
                    //          V
                    //         NULL

                    auto const pointee_class_scope = (*pointee_class)->ref.lock();
                    auto const& syms = pointee_class_scope->instance_var_symbols;

                    for (auto const idx : indices(syms)) {
                        auto const& t = syms[idx]->type;
                        if (t.is_aggregate()) {
                            auto *const instance_var_val
                                = emitter.load_if_ref(
                                    emitter.alloc_helper.create_alloca(t, false), t
                                );
                            auto *const member_access_val =
                                emitter.ctx.builder.CreateConstInBoundsGEP2_32(indexed_ptr_val, 0u, idx);
                            emitter.ctx.builder.CreateStore(
                                    instance_var_val,
                                    member_access_val
                                );
                        }
                    }
                }

                emit_copy_to_lhs(
                    indexed_ptr_val,
                    access->type
                );
            } else if (!access->callee_scope.expired()) {
                auto const callee = access->callee_scope.lock();
                assert(callee->name == "[]=");

                check(
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
                error(access, "Invalid assignment to indexed access");
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
            emit_copy_to_lhs(*emitted, ufcs->type);
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

    // Note:
    // load_if_ref() strips reference if the value is treated by reference.
    // To determine the value is treated by reference, type::type is used.
    llvm::Value *load_if_ref(llvm::Value *const v, type::type const& t)
    {
        if (!t.is_aggregate()) {
            return deref(v, t);
        } else {
            return v;
        }
    }

    template<class Node>
    llvm::Value *load_if_ref(llvm::Value *const v, Node const& hint)
    {
        return load_if_ref(v, type::type_of(hint));
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
        if (elem_type.is_aggregate()) {
            return deref(v, elem_type);
        } else {
            return v;
        }
    }

    llvm::Value *deref(llvm::Value *const v, type::type const& t)
    {
        auto *const ty = type_emitter.emit(t);
        if (ty == v->getType()) {
            return v;
        } else {
            assert(v->getType()->isPointerTy());
            assert(v->getType()->getPointerElementType() == ty);
            return ctx.builder.CreateLoad(v);
        }
    }

    template<class Expr>
    llvm::Value *deref(llvm::Value *const v, Expr const& e)
    {
        return deref(v, type::type_of(e));
    }

public:

    llvm_ir_emitter(std::string const& f, context &c, semantics::semantics_context const& sc, llvm::Module &m)
        : module(&m)
        , ctx(c)
        , semantics_ctx(sc)
        , var_table()
        , file(f)
        , type_emitter(ctx.llvm_context, sc.lambda_captures)
        , gc_emitter(c, type_emitter, *module)
        , member_emitter(ctx)
        , alloc_helper(ctx, type_emitter, gc_emitter, sc.lambda_captures, semantics_ctx, m)
        , inst_emitter(ctx, type_emitter, m)
        , builtin_func_emitter(m, ctx, type_emitter, gc_emitter, inst_emitter)
        , builtin_ctor_emitter(ctx, type_emitter, gc_emitter, alloc_helper, module, *this)
    {}

    llvm_ir_emitter(std::string const& f, context &c, semantics::semantics_context const& sc)
        : llvm_ir_emitter(f, c, sc, *new llvm::Module(f, c.llvm_context))
    {
        module->setDataLayout(ctx.data_layout->getStringRepresentation());
        module->setTargetTriple(ctx.triple.getTriple());
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
            return inst_emitter.emit_unit_constant();
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
            auto *const alloca_inst = alloc_helper.create_alloca(t);

            for (auto const idx : helper::indices(elem_values)) {
                auto const elem_type = type::type_of(elem_exprs[idx]);

                if (auto const copier = semantics_ctx.copier_of(elem_type)) {
                    val const ptr_to_elem = ctx.builder.CreateStructGEP(alloca_inst, idx);
                    val const copied = emit_copier_call(
                            ast::node::location_of(elem_exprs[idx]),
                            elem_values[idx],
                            *copier
                        );
                    ctx.builder.CreateStore(copied, ptr_to_elem);
                } else {
                    alloc_helper.create_deep_copy(
                            elem_values[idx],
                            load_aggregate_elem(
                                ctx.builder.CreateStructGEP(alloca_inst, idx),
                                elem_type
                            ),
                            elem_type
                        );
                }
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

    val emit_array_constant(type::pointer_type const& t, std::vector<ast::node::any_expr> const& elem_exprs)
    {
        auto const& elem_type = t->pointee_type;

        std::vector<val> elem_values;
        elem_values.reserve(elem_exprs.size());
        for (auto const& e : elem_exprs) {
            elem_values.push_back(emit(e));
        }

        auto *const array_ty = llvm::ArrayType::get(type_emitter.emit_alloc_type(elem_type), elem_exprs.size());

        if (all_of(elem_values, [](auto const v) -> bool { return llvm::isa<llvm::Constant>(v); })) {
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

            return ctx.builder.CreateConstInBoundsGEP2_32(constant, 0u, 0u, "array_lit");
        } else {
            // TODO:
            // Now, static arrays are allocated with type [ElemType x N]* (e.g. [int x N]* for static_array(int)).
            // However, I must consider the structure of array.  Because arrays are allocated directly as [T x N], all elements are
            // allocated once and they can't be separated.  This means that when extracting a element from the array, compiler must
            // copy the element instead of copying the pointer to the element.  And GC can't destroy the whole array until all elements
            // are expired.
            // So, the allocation of static array should be [ElemType* x N]*.

            auto *const allocated = gc_emitter.emit_malloc(elem_type, elem_exprs.size(), "arrlit");

            for (auto const idx : helper::indices(elem_exprs)) {
                auto *const elem_value = ctx.builder.CreateConstInBoundsGEP1_32(allocated, idx);

                if (auto const copier = semantics_ctx.copier_of(elem_type)) {
                    val const copied = emit_copier_call(
                            ast::node::location_of(elem_exprs[idx]),
                            elem_values[idx],
                            *copier
                        );

                    // Note:
                    // Copy an element of array because of array structure (see above TODO).
                    ctx.builder.CreateStore(ctx.builder.CreateLoad(copied), elem_value);
                } else {
                    alloc_helper.create_deep_copy(
                            load_if_ref(elem_values[idx], elem_type),
                            load_aggregate_elem(
                                elem_value,
                                elem_type
                            ),
                            elem_type
                        );
                }
            }

            return allocated;
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

    val emit(ast::node::string_literal const& literal)
    {
        assert(literal->type.is_string_class());

        auto *const native_string_value = ctx.builder.CreateGlobalStringPtr(literal->value.c_str());
        auto *const size_value = llvm::ConstantInt::get(
                        llvm::Type::getInt64Ty(ctx.llvm_context),
                        static_cast<std::uint64_t>(literal->value.size()),
                        false
                    );

        return emit_class_object_construct(
                literal,
                *type::get<type::class_type>(literal->type),
                std::vector<val>{
                    native_string_value,
                    size_value
                }
            );
    }

    val emit(ast::node::array_literal const& literal)
    {
        assert(literal->type.is_array_class());

        auto const underlying_type = literal->type.get_array_underlying_type();
        assert(underlying_type);

        auto *const native_array_value = check(
                literal,
                emit_array_constant(
                    *underlying_type,
                    literal->element_exprs
                ),
                "inner static array in array literal"
            );

        return emit_class_object_construct(
                literal,
                *type::get<type::class_type>(literal->type),
                std::vector<val> {
                    load_if_ref(native_array_value, type::type{*underlying_type}),
                    ctx.builder.getInt64(literal->element_exprs.size())
                }
            );
    }

    val emit(ast::node::lambda_expr const& lambda)
    {
        return emit(lambda->receiver);
    }

    llvm::Module *emit(ast::node::inu const& p)
    {
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

        if (auto const copier = semantics_ctx.copier_of(param->type)) {
            auto *const copied = emit_copier_call(param, param_val, *copier);
            register_var(param_sym, copied);
        } else {
            auto const inst
                = check(
                    param,
                    alloc_helper.alloc_and_deep_copy(param_val, param_sym->type, param->name),
                    "allocation for variable parameter"
                );

            register_var(param_sym, inst);
        }
    }

    void emit_instance_var_init_params(scope::func_scope const& ctor, ast::node::function_definition const& def)
    {
        assert(ctor->is_member_func);

        auto const& self_sym = ctor->params[0];
        assert(type::is_a<type::class_type>(self_sym->type));
        auto const receiver_type = *type::get<type::class_type>(self_sym->type);
        assert(!receiver_type->ref.expired());
        auto const clazz = receiver_type->ref.lock();

        auto *const self_val = lookup_var(self_sym);
        assert(self_val);

        for (auto const& param : def->params) {
            auto const param_sym = param->param_symbol.lock();
            auto const name_hint = clazz->name + '.' + param_sym->name;

            if (!param_sym->is_instance_var()) {
                continue;
            }

            auto *const init_val = lookup_var(param_sym);
            assert(init_val);

            auto const offset = clazz->get_instance_var_offset_of(param_sym->name.substr(1u)/* omit '@' */);
            assert(offset);
            auto *const elem_val = ctx.builder.CreateStructGEP(self_val, *offset, name_hint);

            if (auto const copier = semantics_ctx.copier_of(param_sym->type)) {
                auto *const copied = emit_copier_call(param, init_val, *copier);
                ctx.builder.CreateStore(copied, elem_val);
                register_var(param_sym, copied);
            } else {
                auto *const dest_val = load_aggregate_elem(
                        elem_val,
                        param_sym->type
                    );

                assert(dest_val);
                alloc_helper.create_deep_copy(init_val, dest_val, param_sym->type);
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
        auto const maybe_prototype_ir = lookup_func(scope);
        assert(maybe_prototype_ir);
        auto const& prototype_ir = *maybe_prototype_ir;
        auto const block = llvm::BasicBlock::Create(ctx.llvm_context, "entry", prototype_ir);
        ctx.builder.SetInsertPoint(block);

        for (auto const& p : func_def->params) {
            emit(p);
        }

        if (scope->is_ctor()) {
            emit_instance_var_init_params(scope, func_def);
        }

        emit(func_def->body);

        if (ctx.builder.GetInsertBlock()->getTerminator()) {
            // Note:
            // Already terminated. Do nothing.
        } else if (!func_def->ret_type
                || func_def->kind == ast::symbol::func_kind::proc
                || *func_def->ret_type == type::get_unit_type()) {
            ctx.builder.CreateRet(
                inst_emitter.emit_unit_constant()
            );
        } else {
            // Note:
            // Believe that the insert block is the last block of the function
            ctx.builder.CreateUnreachable();
        }

        if (scope->is_main_func()) {
            assert(scope->ret_type);
            emit_program_entry_point(prototype_ir, !scope->params.empty(), *scope->ret_type);
            return;
        }
    }

    template<class Statements>
    bool /* returns if terminated or not */
    emit_block(Statements const& stmts)
    {
        for (auto const& stmt : stmts) {
            if (ctx.builder.GetInsertBlock()->getTerminator()) {
                // Note:
                // If current basic block is already terminated,
                // no more statement should be emitted.
                return true;
            }
            try {
                emit(stmt);
            }
            catch (emit_skipper)
            {}
        }
        return ctx.builder.GetInsertBlock()->getTerminator();
    }

    void emit(ast::node::statement_block const& block)
    {
        emit_block(block->value);
    }

    val emit(ast::node::block_expr const& block)
    {
        bool const terminated = emit_block(block->stmts);
        if (terminated) {
            throw emit_skipper{};
        }
        return emit(block->last_expr);
    }

    val emit(ast::node::if_expr const& if_)
    {
        auto helper = bb_helper(if_);
        std::vector<std::pair<val, llvm::BasicBlock *>> evaluated_blocks;

        auto const emit_block
            = [&, this](auto const& block)
            {
                try {
                    auto *const block_val = emit(block);
                    assert(!ctx.builder.GetInsertBlock()->getTerminator());
                    evaluated_blocks.emplace_back(block_val, ctx.builder.GetInsertBlock());
                }
                catch(emit_skipper) {
                    // Do nothing
                }
            };

        // IR for if-then clause
        val cond_val = load_if_ref(emit(if_->condition), if_->condition);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if.expr.unless"), "unless statement");
        }

        auto *then_block = helper.create_block_for_parent("if.expr.then");
        auto *else_block = helper.create_block("if.expr.else");
        auto *const end_block = helper.create_block("if.expr.end");

        helper.create_cond_br(cond_val, then_block, else_block);

        emit_block(if_->then_block);

        helper.terminate_with_br(end_block);
        helper.append_block(else_block);

        // IR for elseif clause
        for (auto const& elseif : if_->elseif_block_list) {
            cond_val = load_if_ref(emit(elseif.first), elseif.first);
            then_block = helper.create_block_for_parent("elseif.expr.then");
            else_block = helper.create_block("elseif.expr.else");
            helper.create_cond_br(cond_val, then_block, else_block);

            emit_block(elseif.second);

            helper.terminate_with_br(end_block);
            helper.append_block(else_block);
        }

        // IR for else clause
        emit_block(if_->else_block);

        helper.terminate_with_br(end_block);
        helper.append_block(end_block);

        if (!evaluated_blocks.empty()) {
            auto *const phi
                = ctx.builder.CreatePHI(
                        evaluated_blocks[0].first->getType(),
                        evaluated_blocks.size(),
                        "expr.if.phi"
                    );

            for (auto const& block : evaluated_blocks) {
                phi->addIncoming(block.first, block.second);
            }
            return phi;
        } else {
            throw emit_skipper{};
        }
    }

    void emit(ast::node::if_stmt const& if_)
    {
        auto helper = bb_helper(if_);

        // IR for if-then clause
        val cond_val = load_if_ref(emit(if_->condition), if_->condition);
        if (if_->kind == ast::symbol::if_kind::unless) {
            cond_val = check(if_, ctx.builder.CreateNot(cond_val, "if.stmt.unless"), "unless statement");
        }

        auto *then_block = helper.create_block_for_parent("if.stmt.then");
        auto *else_block = helper.create_block("if.stmt.else");

        helper.create_cond_br(cond_val, then_block, else_block);

        auto *const end_block = helper.create_block("if.stmt.end");
        emit(if_->then_stmts);

        helper.terminate_with_br(end_block);
        helper.append_block(else_block);

        // IR for elseif clause
        for (auto const& elseif : if_->elseif_stmts_list) {
            cond_val = load_if_ref(emit(elseif.first), elseif.first);
            then_block = helper.create_block_for_parent("elseif.stmt.then");
            else_block = helper.create_block("elseif.stmt.else");
            helper.create_cond_br(cond_val, then_block, else_block);

            emit(elseif.second);

            helper.terminate_with_br(end_block);
            helper.append_block(else_block);
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

            auto const enumerate_arg_types
                = [&param_types]
                {
                    if (param_types.empty()) {
                        return std::string{};
                    }

                    std::string msg = "\n  Note: Argument type(s): ";
                    for (auto const& t : param_types) {
                        msg += t.to_string() + ',';
                    }
                    return msg;
                };

            return check(
                    n,
                    builtin_func_emitter.emit(scope->name, param_types),
                    boost::format("builtin function '%1%'%2%") % scope->to_string() % enumerate_arg_types()
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
            assert(args.back());
        }

        auto const child_type = type::type_of(invocation->child);

        if (auto const func = type::get<type::func_type>(child_type)) {
            return check(
                    invocation,
                    ctx.builder.CreateCall(
                        load_if_ref(emit(invocation->child), child_type),
                        args
                    ),
                    "invoking function type value"
                );
        }

        auto const generic = type::get<type::generic_func_type>(child_type);
        if (!generic) {
            // TODO:
            // Deal with func_type
            error(invocation, boost::format("calling '%1%' type variable which is not callable") % child_type.to_string());
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

    template<class Node>
    val emit_copier_call(
            Node const& node,
            val const src_value,
            scope::func_scope const& callee
    ) {
        return check(
                node,
                ctx.builder.CreateCall(
                    emit_non_builtin_callee(node, callee),
                    src_value
                ),
                "copier call"
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
            return inst_emitter.emit_unit_constant();
        } else {
            error(var, boost::format("Invalid variable reference '%1%'. Its type is '%2%'") % var->name % var->type.to_string());
        }
    }

    val emit(ast::node::index_access const& access)
    {
        auto *const child_val = load_if_ref(emit(access->child), access->child); // Note: Load inst for pointer
        auto *const index_val = load_if_ref(emit(access->index_expr), access->index_expr);

        if (!access->callee_scope.expired()) {
            auto const callee = access->callee_scope.lock();
            assert(!callee->is_anonymous());
            assert(!callee->is_builtin);

            return check(
                    access,
                    ctx.builder.CreateCall2(
                        emit_non_builtin_callee(access, callee),
                        child_val,
                        index_val
                    ),
                    "user-defined index access operator"
                );
        }

        auto const result = inst_emitter.emit_builtin_element_access(
                child_val,
                index_val,
                type::type_of(access->child)
            );

        if (auto const err = result.get_error()) {
            error(access, *err);
        }

        return result.get_unsafe();
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
            return inst_emitter.emit_unit_constant();
        }

        val const elem_ptr = ctx.builder.CreateStructGEP(child_val, capture->offset, "capture." + ufcs->member_name);

        // Note:
        // (a : ([int], double))[0]: (i64*, double)* -> i64** -> i64*
        return ufcs->type.is_builtin() ? elem_ptr : ctx.builder.CreateLoad(elem_ptr);
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

        if (auto const maybe_idx = offset_of(ufcs->member_name)) {
            auto const& idx = *maybe_idx;

            return ctx.builder.CreateStructGEP(child_val, idx);
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
                // Note:
                // Dereference the value because emit_instance_var_access() doesn't dereference value
                // because it is used both lhs of assign and here.
                return deref(*v, ufcs->type);
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

    void emit_tuple_traverse(ast::node::for_stmt const& for_, type::tuple_type const& tuple, val const range_value)
    {
        // Note:
        // From:
        //   for e in (1, 'a')
        //      ...
        //   end
        //
        // To:
        //  do
        //      e := {define symbol for the 1st element of (1, 'a')}
        //      ...
        //  end
        //  do
        //      e := {define symbol for the 2nd element of (1, 'a')}
        //      ...
        //  end
        //

        assert(for_->body_stmts->value.size() == tuple->element_types.size());

        for (auto const idx : helper::indices(tuple->element_types)) {
            auto const& elem_type = tuple->element_types[idx];
            auto const block = get_as<ast::node::statement_block>(for_->body_stmts->value[idx]);
            assert(block);

            auto const scope = (*block)->scope.lock();
            auto const elem_access
                = inst_emitter.emit_builtin_element_access(
                        range_value,
                        ctx.builder.getInt64(idx),
                        tuple
                    );

            if (auto const err = elem_access.get_error()) {
                error(for_, *err);
            }

            auto *const elem_access_value = elem_access.get_unsafe();

            auto const emit_iter_var
                = [&, this](auto const& param, val const the_value)
                {
                    auto const iter_symbol = helper::find_if(
                            scope->local_vars,
                            [&](auto const& s){ return s->name == param->name; }
                        );
                    assert(iter_symbol);
                    auto const& sym = *iter_symbol;

                    if (sym->immutable) {
                        register_var(sym, the_value);
                        the_value->setName(sym->name);
                    } else {
                        if (auto const copier = semantics_ctx.copier_of(sym->type)) {
                            val const copied = emit_copier_call(param, the_value, *copier);
                            copied->setName(sym->name);
                            register_var(sym, copied);
                        } else {
                            auto *const allocated = alloc_helper.alloc_and_deep_copy(the_value, sym->type, sym->name);
                            assert(allocated);
                            register_var(sym, allocated);
                        }
                    }
                };

            if (for_->iter_vars.size() == 1) {
                emit_iter_var(for_->iter_vars[0], elem_access_value);
            } else {
                auto const elem_tuple = type::get<type::tuple_type>(elem_type);
                assert(elem_tuple && (*elem_tuple)->element_types.size() == for_->iter_vars.size());

                for (auto const i : helper::indices(for_->iter_vars)) {
                    auto const& p = for_->iter_vars[i];
                    auto const elem_elem_access
                        = inst_emitter.emit_builtin_element_access(
                                elem_access_value,
                                ctx.builder.getInt64(i),
                                *elem_tuple
                            );


                    if (auto const err = elem_elem_access.get_error()) {
                        error(p, *err);
                    }

                    emit_iter_var(p, elem_elem_access.get_unsafe());
                }
            }

            emit(*block);
        }
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
        if (auto const tuple = type::get<type::tuple_type>(range_type)) {
            emit_tuple_traverse(for_, *tuple, range_val);
            return;
        }

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
            param->is_var && semantics_ctx.copier_of(param->type)
                ? alloc_helper.create_alloca(param->type, false /*zero init?*/, param->name)
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
                            loaded_counter_val,
                            param->name
                        )
                    : check(
                            for_,
                            ctx.builder.CreateCall2(
                                emit_non_builtin_callee(for_, for_->index_callee_scope.lock()),
                                range_val,
                                loaded_counter_val,
                                param->name
                            ),
                            "index access call for 'for' statement"
                        )
                ;

            auto const s = sym.lock();

            if (auto const copier = semantics_ctx.copier_of(s->type)) {
                auto *const copied = emit_copier_call(
                            param, elem_ptr_val, *copier
                        );
                copied->setName(param->name);
                register_var(s, copied);
            } else if (allocated) {
                alloc_helper.create_deep_copy(elem_ptr_val, allocated, s->type);
                register_var(s, allocated);
            } else {
                register_var(s, elem_ptr_val);
                elem_ptr_val->setName(param->name);
            }
        }

        emit(for_->body_stmts);

        ctx.builder.CreateStore(ctx.builder.CreateAdd(loaded_counter_val, ctx.builder.getInt64(1u), "incremented"), counter_val);
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
                auto *const allocated = alloc_helper.create_alloca(sym->type);
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

                    auto *const ptr_to_instance_var = ctx.builder.CreateStructGEP(self_val, *offset);

                    if (auto const copier = semantics_ctx.copier_of(type)) {
                        ctx.builder.CreateStore(
                                emit_copier_call(
                                    decl, value, *copier
                                ),
                                ptr_to_instance_var
                            );
                        return;
                    }

                    auto *const dest_val = load_aggregate_elem(ptr_to_instance_var, type);
                    assert(dest_val);
                    dest_val->setName(decl->name);

                    alloc_helper.create_deep_copy(value, dest_val, type);

                } else if (decl->is_var) {
                    if (auto const copier = semantics_ctx.copier_of(type)) {
                        val const copied = emit_copier_call(decl, value, *copier);
                        copied->setName(decl->name);
                        register_var(std::move(sym), copied);
                    } else {
                        auto *const allocated = alloc_helper.alloc_and_deep_copy(value, type, sym->name);
                        assert(allocated);
                        register_var(std::move(sym), allocated);
                    }
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
                lhs_of_assign_emitter<self, decltype(assign)>{*this, rhs_value, assign}.emit(lhs_expr);
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

    // Note:
    // Create a function wrapping non-captured lambda function.
    // It omits capture struct and forwards parameters to inner lambda function call
    template<class Location>
    llvm::Function *emit_non_capture_lambda_wrapped_func(scope::func_scope const& non_capture_lambda, Location const& location)
    {
        auto const lambda_name = non_capture_lambda->to_string();

        {
            auto *const already_made = module->getFunction(lambda_name + ".wrapped");
            if (already_made) {
                return already_made;
            }
        }

        auto *const lambda_func_val = module->getFunction(lambda_name);
        if (!lambda_func_val) {
            error(location, "invalid lambda function");
        }

        // Note:
        // In LLVM 3.6 or later, llvm::FunctionType has params() member function

        auto *const lambda_func_ty = lambda_func_val->getFunctionType();

        auto const num_params = lambda_func_ty->getNumParams();
        assert(num_params > 0);

        auto *const receiver_ty = llvm::dyn_cast<llvm::StructType>(lambda_func_ty->getParamType(0u)->getPointerElementType());
        assert(receiver_ty);

        if (receiver_ty->getNumElements() != 0u) {
            error(
                location,
                "incompatible lambda type to function type conversion"
            );
        }

        std::vector<llvm::Type *> param_tys;
        for (auto i = 1u; i < num_params; ++i) {
            param_tys.push_back(lambda_func_ty->getParamType(i));
        }

        // Note:
        // Create a prototype of the wrapped function

        auto *const wrapped_func_ty = llvm::FunctionType::get(
                lambda_func_ty->getReturnType(),
                param_tys,
                false
            );

        auto *const wrapped_func_val = llvm::Function::Create(
                wrapped_func_ty,
                llvm::Function::InternalLinkage,
                lambda_name + ".wrapped",
                module
            );

        wrapped_func_val->addFnAttr(llvm::Attribute::NoUnwind);
        wrapped_func_val->addFnAttr(llvm::Attribute::InlineHint);

        std::vector<val> arg_vals = {inst_emitter.emit_unit_constant()};
        for (auto src_itr = ++lambda_func_val->arg_begin(),
                  dst_itr = wrapped_func_val->arg_begin();
             src_itr != lambda_func_val->arg_end();
             ++src_itr, ++dst_itr) {
            dst_itr->setName(src_itr->getName());
            arg_vals.push_back(dst_itr);
        }

        // Note:
        // Create body of the wrapped function
        auto const saved_insert_point = ctx.builder.GetInsertBlock();
        auto const body = llvm::BasicBlock::Create(ctx.llvm_context, "entry", wrapped_func_val);
        ctx.builder.SetInsertPoint(body);

        ctx.builder.CreateRet(
                ctx.builder.CreateCall(
                    lambda_func_val,
                    arg_vals
                )
            );

        ctx.builder.SetInsertPoint(saved_insert_point);
        return wrapped_func_val;
    }

    val emit(ast::node::cast_expr const& cast)
    {
        auto const child_type = type::type_of(cast->child);
        auto *const child_val = load_if_ref(emit(cast->child), child_type);
        if (cast->type == child_type) {
            return child_val;
        }

        if (auto const callee = cast->callee_cast_scope.lock()) {
            return check(
                    cast,
                    ctx.builder.CreateCall(
                        emit_non_builtin_callee(cast, callee),
                        child_val
                    ),
                    "invalid cast function call"
                );
        }

        if (type::is_a<type::pointer_type>(child_type)) {
            if (cast->type.is_builtin("uint")) {
                assert(child_val->getType()->isPointerTy());
                return ctx.builder.CreatePtrToInt(child_val, ctx.builder.getInt64Ty());
            }
        }

        if (auto const casted_func = cast->casted_func_scope.lock()) {
            if (casted_func->is_anonymous()) {
                return emit_non_capture_lambda_wrapped_func(casted_func, cast);
            }

            return check(
                    cast,
                    module->getFunction(casted_func->to_string()),
                    "invalid target function in cast to function type"
                );
        }

        auto const cast_error
            = [&]
            {
                error(
                    cast,
                    boost::format("Cannot cast from '%1%' to '%2%'")
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

    template<class Construction>
    val emit_class_object_construct(Construction const& obj, type::class_type const& t, std::vector<val> && arg_values)
    {
        auto const obj_val = alloc_helper.create_alloca(t);

        arg_values.insert(std::begin(arg_values), obj_val);

        assert(!obj->callee_ctor_scope.expired());
        auto const ctor_scope = obj->callee_ctor_scope.lock();

        ctx.builder.CreateCall(
                emit_non_builtin_callee(obj, ctor_scope),
                std::move(arg_values),
                "ctor.call"
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

        return check(obj, builtin_ctor_emitter.emit(obj->type, arg_vals, obj), "object construction");
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
    if (llvm::verifyModule(the_module, llvm::ReturnStatusAction, &errmsg) && !errmsg.empty()) {

#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
    llvm::raw_string_ostream rso(errmsg);
    if (llvm::verifyModule(the_module, &rso) && !errmsg.empty()) {

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
