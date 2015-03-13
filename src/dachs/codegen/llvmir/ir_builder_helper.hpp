#if !defined DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED

#include <memory>
#include <algorithm>
#include <utility>
#include <cstddef>

#include <boost/range/irange.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {
namespace builder {

using boost::adaptors::filtered;
using boost::irange;

template<class Node>
class block_branch_helper {
    using node_type = std::shared_ptr<Node>;
    using parent_ptr_type = decltype(std::declval<llvm::IRBuilder<>>().GetInsertBlock()->getParent());

    node_type const& node;
    context &ctx;
    parent_ptr_type const parent;

    template<class String>
    void error(String const& msg) const
    {
        // TODO:
        // Dump builder's debug information and context's information
        throw code_generation_error{
                "LLVM IR generator",
                (boost::format("In line:%1%:col:%2%, %3%") % node->line % node->col % msg).str()
            };
    }

    template<class T, class String>
    T check(T const v, String const& feature) const
    {
        if (!v) {
            error(std::string{"Failed to create "} + feature);
        }
        return v;
    }

public:
    block_branch_helper(node_type const& n, context &c) noexcept
        : node(n), ctx(c), parent(c.builder.GetInsertBlock()->getParent())
    {}

    parent_ptr_type get_parent() const noexcept
    {
        return parent;
    }

    bool has_parent() const noexcept
    {
        return parent;
    }

    // If current block is not terminated, create br
    llvm::BranchInst *terminate_with_br(llvm::BasicBlock *const dest, llvm::BasicBlock *const next = nullptr)
    {
        llvm::BranchInst *br = nullptr;

        if (!ctx.builder.GetInsertBlock()->getTerminator()) {
            br = check(ctx.builder.CreateBr(dest), "branch instruction");
        }

        if (next) {
            ctx.builder.SetInsertPoint(next);
        }
        return br;
    }

    void append_block(llvm::BasicBlock *const b)
    {
        if (!parent) {
            error("No parent found");
        }

        auto *current_block = ctx.builder.GetInsertBlock();
        parent->getBasicBlockList().insertAfter(current_block, b);
        ctx.builder.SetInsertPoint(b);
    }

    void append_block(llvm::BasicBlock *const b, llvm::BasicBlock *const next)
    {
        if (!parent) {
            error("No parent found");
        }

        auto *const current_block = ctx.builder.GetInsertBlock();
        parent->getBasicBlockList().insertAfter(current_block, b);
        if (next) {
            ctx.builder.SetInsertPoint(next);
        }
    }

    llvm::BranchInst *create_br(llvm::BasicBlock *const b)
    {
        auto const br = check(ctx.builder.CreateBr(b), "branch instruction");
        ctx.builder.SetInsertPoint(b);
        return br;
    }

    llvm::BranchInst *create_br(llvm::BasicBlock *const b, llvm::BasicBlock *const next)
    {
        auto const br = check(ctx.builder.CreateBr(b), "branch instruction");
        if (next) {
            ctx.builder.SetInsertPoint(next);
        }
        return br;
    }

    llvm::BranchInst *create_cond_br(llvm::Value *const cond_val, llvm::BasicBlock *const if_true, llvm::BasicBlock *const if_false, llvm::BasicBlock *const next)
    {
        auto const cond_br = check(ctx.builder.CreateCondBr(cond_val, if_true, if_false), "condition branch");
        if (next) {
            ctx.builder.SetInsertPoint(next);
        }
        return cond_br;
    }

    llvm::BranchInst *create_cond_br(llvm::Value *const cond_val, llvm::BasicBlock *const if_true, llvm::BasicBlock *const if_false)
    {
        auto const cond_br = check(ctx.builder.CreateCondBr(cond_val, if_true, if_false), "condition branch");
        ctx.builder.SetInsertPoint(if_true);
        return cond_br;
    }

    template<class String>
    llvm::BasicBlock *create_block_for_parent(String const& name = "", bool const move_to_the_block = false)
    {
        if (!parent) {
            error("No parent found");
        }

        auto const the_block = check(llvm::BasicBlock::Create(ctx.llvm_context, name, parent), "basic block");
        if (move_to_the_block) {
            ctx.builder.SetInsertPoint(the_block);
        }
        return the_block;
    }

    template<class String = char const* const>
    auto create_block(String const& name = "", bool const move_to_the_block = false) const
    {
        auto const the_block = check(llvm::BasicBlock::Create(ctx.llvm_context, name), "basic block");
        if (move_to_the_block) {
            ctx.builder.SetInsertPoint(the_block);
        }
        return the_block;
    }

};

class inst_emit_helper {
    context &ctx;

    using value_or_error_type = boost::variant<llvm::Value *, std::string>;

    template<class Format, class T>
    auto format_impl(Format && fmt, T const& v) const
    {
        return std::forward<Format>(fmt) % v;
    }

    template<class Format, class Head, class... Tail>
    auto format_impl(Format && fmt, Head const& head, Tail const&... tail) const
    {
        return format_impl(std::forward<Format>(fmt) % head, tail...);
    }

    template<class String, class... Args>
    std::string format(String && str, Args const&... args) const
    {
        return format_impl(boost::format(std::forward<String>(str)), args...).str();
    }

public:

    explicit inst_emit_helper(context &c)
        : ctx(c)
    {}

    llvm::Value *emit_elem_value(llvm::Value *const v, type::type const& t)
    {
        if (t.is_builtin()) {
            return v;
        }

        // Note:
        // e.g. getelementptr {[1 x i64]* }* %x, i32 0, i32 0
        assert(v->getType()->getPointerElementType()->isPointerTy());
        return ctx.builder.CreateLoad(v);
    }

    value_or_error_type emit_builtin_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::type const& t)
    {
        index->setName("idx");
        if (auto const tuple = type::get<type::tuple_type>(t)) {
            return emit_builtin_element_access(aggregate, index, *tuple);
        } else if (auto const array = type::get<type::array_type>(t)) {
            return emit_builtin_element_access(aggregate, index, *array);
        } else if (auto const builtin = type::get<type::builtin_type>(t)) {
            if ((*builtin)->name == "string") {
                return emit_builtin_element_access(aggregate, index, *builtin);
            }
        }

        return "Value is not tuple, array and string";
    }

    value_or_error_type emit_builtin_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::tuple_type const& t)
    {
        assert(aggregate->getType()->isPointerTy());
        assert(aggregate->getType()->getPointerElementType()->isStructTy());

        auto *const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index);
        if (!constant_index) {
            return "Index is not a constant";
        }

        auto const idx = constant_index->getZExtValue();
        assert(t->element_types.size() > idx);
        return emit_elem_value(ctx.builder.CreateStructGEP(aggregate, idx), t->element_types[idx]);
    }

    value_or_error_type emit_builtin_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::array_type const& t)
    {
        assert(aggregate->getType()->isPointerTy());
        auto const ty = aggregate->getType()->getPointerElementType();

        if (ty->isPointerTy() && ty->getPointerElementType()->isIntegerTy(8u)) {
            assert(t->element_type.is_builtin("string"));

            // Note:
            // Corner case.  When i8** (it means the argument of 'main')
            return ctx.builder.CreateLoad(
                ctx.builder.CreateInBoundsGEP(
                        aggregate,
                        index
                    )
                );
        }

        assert(ty->isArrayTy());

        auto *const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index);
        if (constant_index && t->size) {
            auto const idx = constant_index->getZExtValue();
            auto const size = *t->size;

            if (idx >= size) {
                return format("Array index is out of bounds %1% for 0..%2%", idx, size);
            }
        }

        return emit_elem_value(
                ctx.builder.CreateInBoundsGEP(
                    aggregate,
                    index
                ), t->element_type
            );
    }

    value_or_error_type emit_builtin_element_access(llvm::Value *str_val, llvm::Value *const index, type::builtin_type const& t)
    {
        // Note:
        // Workaround.  At first, allocate i8* and store the global string pointer
        // to the allocated memory. At second, load it and call CreateGEP().
        // This make CreateGEP() not to fold the getelementptr inst.
        // Without the workaround, the global string pointer is a constant and
        // when the index value is a constant, the getelementptr inst will be folded.
        // However, it seems that the result of folding the getelementptr is treated as if
        // it is not a getelementptr inst.  The result of llvm::isa<llvm::GetElementPtrInst>(result)
        // returns false.  I don't know why.

        assert(t->name == "string");
        (void) t;

        if (str_val->getType()->getPointerElementType()->isPointerTy()) {
            str_val = ctx.builder.CreateLoad(str_val);
        }
        auto const str_ptr = ctx.builder.CreateAlloca(str_val->getType());
        ctx.builder.CreateStore(str_val, str_ptr);

        return ctx.builder.CreateGEP(
                ctx.builder.CreateLoad(str_ptr),
                index
            );
    }
};

class alloc_helper {
    context &ctx;
    type_ir_emitter &type_emitter;
    semantics::lambda_captures_type const& lambda_captures;

public:

    alloc_helper(context &c, type_ir_emitter &e, decltype(lambda_captures) const& cs)
        : ctx(c), type_emitter(e), lambda_captures(cs)
    {}

    template<class Ptr>
    llvm::CallInst *create_memset(Ptr *const p, llvm::Type *const ty, type::type const& t)
    {
        auto const emit_memset
            = [p, ty, this](auto const size, auto const align)
            {
                return ctx.builder.CreateMemSet(
                        p,
                        ctx.builder.getInt8(0u),
                        size,
                        align
                    );
            };

        if (type::is_a<type::array_type>(t)) {
            auto *const elem_ty = ty->getPointerElementType();
            assert(elem_ty);
            auto const size = ctx.data_layout->getTypeAllocSize(elem_ty);
            auto *const array_ty = llvm::ArrayType::get(elem_ty, size);

            return emit_memset(
                    size,
                    ctx.data_layout->getPrefTypeAlignment(array_ty)
                );
        } else {
            return emit_memset(
                    ctx.data_layout->getTypeAllocSize(ty),
                    ctx.data_layout->getPrefTypeAlignment(ty)
                );
        }
    }

    template<class Dest, class Src>
    llvm::CallInst *create_memcpy_array(Dest *const dest_val, Src *const src_val, type::array_type const& t)
    {
        assert(dest_val->getType()->isPointerTy());
        assert(src_val->getType()->isPointerTy());
        assert(t->size);

        auto const ty = dest_val->getType()->getPointerElementType();
        auto const elem_size = ctx.data_layout->getTypeAllocSize(ty);
        auto const array_ty = llvm::ArrayType::get(ty, elem_size);
        return ctx.builder.CreateMemCpy(
                dest_val,
                src_val,
                elem_size * (*t->size),
                ctx.data_layout->getPrefTypeAlignment(array_ty)
            );
    }

    template<class String = char const* const>
    llvm::Value *create_alloca_impl(type::type const& t, String const& name = "")
    {
        if (auto const a = type::get<type::array_type>(t)) {
            auto *const allocated = ctx.builder.CreateAlloca(type_emitter.emit_alloc_fixed_array(*a), nullptr /*size*/, name);
            return ctx.builder.CreateConstInBoundsGEP2_32(allocated, 0u, 0u);
        } else {
            return ctx.builder.CreateAlloca(type_emitter.emit_alloc_type(t), nullptr/*size*/, name);
        }
    }

    template<class String = char const* const>
    llvm::Value *create_alloca(type::type const& t, bool const init_by_zero = true, String const& name = "")
    {
        auto *const allocated = create_alloca_impl(t, name);

        if (init_by_zero) {
            create_memset(allocated, allocated->getType(), t);
        }

        if (auto const array = type::get<type::array_type>(t)) {
            auto const& elem_type = (*array)->element_type;
            if (elem_type.is_builtin()) {
                return allocated;
            }

            assert((*array)->size);
            for (uint32_t const idx : helper::indices(*(*array)->size)) {
                ctx.builder.CreateStore(
                        create_alloca(elem_type, init_by_zero),
                        ctx.builder.CreateConstInBoundsGEP1_32(allocated, idx)
                    );
            }
        } else if (auto const tuple = type::get<type::tuple_type>(t)) {
            // TODO:
            // If all members are built-in type, use memcpy to copy all elements
            for (auto const idx : helper::indices((*tuple)->element_types)) {
                auto const& elem_type = (*tuple)->element_types[idx];
                if (elem_type.is_builtin()) {
                    continue;
                }

                ctx.builder.CreateStore(
                        create_alloca(elem_type, init_by_zero),
                        ctx.builder.CreateStructGEP(allocated, idx)
                    );
            }
        } else if (auto const clazz = type::get<type::class_type>(t)) {
            auto const& c = *clazz;
            assert(c->param_types.empty());
            auto const scope = c->ref.lock();
            assert(!scope->is_template());
            for (auto const idx : helper::indices(scope->instance_var_symbols)) {
                auto const& var_type = scope->instance_var_symbols[idx]->type;
                if (var_type.is_builtin()) {
                    continue;
                }

                ctx.builder.CreateStore(
                        create_alloca(var_type, init_by_zero),
                        ctx.builder.CreateStructGEP(allocated, idx)
                    );
            }
        } else if (auto const generic_func = type::get<type::generic_func_type>(t)) {
            auto const& g = *generic_func;
            if (!g->ref || g->ref->expired()) {
                return allocated;
            }

            auto const itr = lambda_captures.find(g);
            if (itr == std::end(lambda_captures)) {
                return allocated;
            }

            auto const& captures = itr->second;
            for (auto const& capture : captures) {
                auto const& capture_type = capture.introduced->type;
                if (capture_type.is_builtin()) {
                    continue;
                }

                ctx.builder.CreateStore(
                        create_alloca(capture_type, init_by_zero),
                        ctx.builder.CreateStructGEP(allocated, capture.offset)
                    );
            }
        }

        return allocated;
    }

    template<class V, class String = char const* const>
    llvm::Value *alloc_and_deep_copy(V *const from, type::type const& t, String const& name = "")
    {
        // TODO:
        // Do allocate and copy from 'from' at once.
        // It enables to use memcpy instead of load and store insts for each element of aggregates
        // by calling memset() at first and then emitting alloc inst for each aggregate elements.
        auto *const allocated = create_alloca(t, false, name);
        assert(allocated);

        create_deep_copy(from, allocated, t);

        return allocated;
    }

    template<class V1, class V2>
    void copy_builtin_value(type::builtin_type const& b, V1 *const from, V2 *const to)
    {
        if (b->name != "string" && from->getType()->isPointerTy()) {
            auto const loaded = ctx.builder.CreateLoad(from);
            ctx.builder.CreateStore(loaded, to);
        } else if (b->name == "string" && from->getType()->getPointerElementType()->isPointerTy()) {
            auto const loaded = ctx.builder.CreateLoad(from);
            ctx.builder.CreateStore(loaded, to);
        } else {
            ctx.builder.CreateStore(from, to);
        }
    }

    template<class V1, class V2>
    void create_deep_copy_impl(V1 *const from, V2 *const to, type::type const& t)
    {
        assert(to->getType()->isPointerTy());
        assert(from->getType()->isPointerTy());
        assert(!t.is_builtin());

        if (auto const tuple_ = type::get<type::tuple_type>(t)) {
            auto const& tuple = *tuple_;

            for (auto const idx : helper::indices(tuple->element_types)) {
                auto const& elem_type = tuple->element_types[idx];
                auto *const elem_from = ctx.builder.CreateStructGEP(from, idx);
                auto *const elem_to = ctx.builder.CreateStructGEP(to, idx);

                if (auto const b = type::get<type::builtin_type>(elem_type)) {
                    copy_builtin_value(*b, elem_from, elem_to);
                    continue;
                }

                create_deep_copy(ctx.builder.CreateLoad(elem_from), ctx.builder.CreateLoad(elem_to), elem_type);
            }
        } else if (auto const array_ = type::get<type::array_type>(t)) {
            auto const& array = *array_;
            auto const& elem_type = array->element_type;

            if (elem_type.is_builtin()) {
                create_memcpy_array(to, from, array);
                return;
            }

            assert(array->size);

            for (uint64_t const idx : helper::indices(*array->size)) {
                auto *const elem_from = ctx.builder.CreateLoad(
                            ctx.builder.CreateConstInBoundsGEP1_32(from, idx)
                        );
                auto *const elem_to = ctx.builder.CreateLoad(
                            ctx.builder.CreateConstInBoundsGEP1_32(to, idx)
                        );

                create_deep_copy(elem_from, elem_to, elem_type);
            }
        } else if (auto const clazz_ = type::get<type::class_type>(t)) {
            auto const& clazz = *clazz_;
            assert(clazz->param_types.empty());
            auto const scope = clazz->ref.lock();
            assert(!scope->is_template());

            for (auto const idx : helper::indices(scope->instance_var_symbols)) {
                auto const& var_type = scope->instance_var_symbols[idx]->type;
                auto *const elem_from = ctx.builder.CreateStructGEP(from, idx);
                auto *const elem_to = ctx.builder.CreateStructGEP(to, idx);

                if (auto const b = type::get<type::builtin_type>(var_type)) {
                    copy_builtin_value(*b, elem_from, elem_to);
                    continue;
                }

                create_deep_copy(ctx.builder.CreateLoad(elem_from), ctx.builder.CreateLoad(elem_to), var_type);
            }
        } else if (auto const generic_func = type::get<type::generic_func_type>(t)){
            auto const& g = *generic_func;
            if (!g->ref || g->ref->expired()) {
                return;
            }

            auto const itr = lambda_captures.find(g);
            if (itr == std::end(lambda_captures)) {
                return;
            }

            auto const& captures = itr->second;
            for (auto const& capture : captures) {
                auto const& capture_type = capture.introduced->type;
                auto *const elem_from = ctx.builder.CreateStructGEP(from, capture.offset);
                auto *const elem_to = ctx.builder.CreateStructGEP(to, capture.offset);

                if (auto const b = type::get<type::builtin_type>(capture_type)) {
                    copy_builtin_value(*b, elem_from, elem_to);
                    continue;
                }

                create_deep_copy(ctx.builder.CreateLoad(elem_from), ctx.builder.CreateLoad(elem_to), capture_type);
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    template<class V1, class V2>
    void create_deep_copy(V1 *const from, V2 *const to, type::type const& t)
    {
        if (auto const builtin = type::get<type::builtin_type>(t)) {
            copy_builtin_value(*builtin, from, to);
            return;
        }

        create_deep_copy_impl(from, to, t);
    }

};

} // namespace builder
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
