#if !defined DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED

#include <memory>
#include <algorithm>
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

    value_or_error_type emit_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::type const& t)
    {
        if (auto const tuple = type::get<type::tuple_type>(t)) {
            return emit_element_access(aggregate, index, *tuple);
        } else if (auto const array = type::get<type::array_type>(t)) {
            return emit_element_access(aggregate, index, *array);
        } else if (auto const builtin = type::get<type::builtin_type>(t)) {
            if ((*builtin)->name == "string") {
                return emit_element_access(aggregate, index, *builtin);
            }
        }

        return "Value is not tuple, array and string";
    }

    value_or_error_type emit_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::tuple_type const& t)
    {
        assert(aggregate->getType()->isPointerTy());
        assert(aggregate->getType()->getPointerElementType()->isStructTy());

        auto *const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index);
        if (!constant_index) {
            return "Index is not a constant";
        }

        auto const idx = constant_index->getZExtValue();
        auto result = ctx.builder.CreateStructGEP(aggregate, idx);

        if (!t->element_types[idx].is_builtin()) {
            result = ctx.builder.CreateLoad(result);
        }

        return result;
    }

    value_or_error_type emit_element_access(llvm::Value *const aggregate, llvm::Value *const index, type::array_type const& t)
    {
        auto const ty = aggregate->getType();
        assert(ty->isPointerTy());
        assert(ty->getPointerElementType()->isArrayTy());

        auto const array_ty = llvm::dyn_cast<llvm::ArrayType>(ty->getPointerElementType());

        auto *const constant_index = llvm::dyn_cast<llvm::ConstantInt>(index);
        if (constant_index) {
            assert(array_ty);
            auto const idx = constant_index->getZExtValue();
            auto const size = array_ty->getArrayNumElements();

            if (idx >= size) {
                return format("Array index is out of bounds %1% for 0..%2%", idx, size);
            }
        }

        if (ty->getPointerElementType()->isPointerTy()
                && ty->getPointerElementType()->getPointerElementType()->isIntegerTy(8u)) {
            // Note:
            // Corner case.  When i8** (it means the argument of 'main')
            return ctx.builder.CreateInBoundsGEP(
                        aggregate,
                        index
                    );
        }

        auto result = ctx.builder.CreateInBoundsGEP(
                aggregate,
                (llvm::Value *[2]){
                    ctx.builder.getInt64(0u),
                    index
                }
            );

        if (t->element_type.is_builtin()) {
            result = ctx.builder.CreateLoad(result);
        }
        return result;
    }

    value_or_error_type emit_element_access(llvm::Value *str_val, llvm::Value *const index, type::builtin_type const& t)
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

    template<class T>
    bool is_aggregate_ptr(T const *const t) const noexcept
    {
        if (!t->isPointerTy()) {
            return false;
        }

        auto *const elem_type = t->getPointerElementType();

        if (auto const struct_type = llvm::dyn_cast<llvm::StructType>(elem_type)) {
            if (struct_type->hasName()) {
                // Note: User-defined type
                return false;
            }
        }

        return elem_type->isAggregateType();
    }

public:
    explicit alloc_helper(context &c)
        : ctx(c)
    {}

    template<class FromValue, class String = char const* const>
    llvm::AllocaInst *create_alloca(FromValue *const from, llvm::Value *const array_size = nullptr, String const& name = "")
    {
        auto *const type = from->getType();
        // Note:
        // Absorb the difference between value types and reference types
        return ctx.builder.CreateAlloca(
                llvm::isa<llvm::AllocaInst>(from) || llvm::isa<llvm::GetElementPtrInst>(from) ?
                    type->getPointerElementType()
                    : type
                , array_size
                , name
            );
    }

    template<class String = char const* const>
    llvm::AllocaInst *alloc_and_deep_copy(llvm::Value *const from, String const& name = "")
    {
        auto *const allocated = create_alloca(from, nullptr, name);
        if (!allocated) {
            return nullptr;
        }

        create_deep_copy(from, allocated);
        return allocated;
    }

    // Note:
    // Pointer depth examples:
    //      i8 : 0
    //     *i8 : 1
    //    **i8 : 2
    template<class Type>
    std::size_t pointer_depth(Type const* const type) const noexcept
    {
        return type->isPointerTy()
            ? 1u + pointer_depth(type->getPointerElementType())
            : 0u;
    }

    template<class PtrTypeType>
    void create_deep_copy(llvm::Value *const from, PtrTypeType *const to)
    {
        assert(from);
        assert(to);
        assert(to->getType()->isPointerTy());
        auto *const from_type = from->getType();

        if (is_aggregate_ptr(from_type)) {
            auto *const aggregate_type = from_type->getPointerElementType();
            // Note:
            // memcpy is shallow copy
            ctx.builder.CreateMemCpy(
                to,
                from,
                ctx.data_layout->getTypeAllocSize(aggregate_type),
                ctx.data_layout->getPrefTypeAlignment(aggregate_type)
            );
            if (auto *const struct_type = llvm::dyn_cast<llvm::StructType>(aggregate_type)) {
                for (uint64_t const idx : irange(0u, struct_type->getNumElements())) {
                    if (!struct_type->getElementType(idx)->isPointerTy()) {
                        // Note:
                        // If element type is not pointer, it is already copied by memcpy()
                        continue;
                    }

                    auto *const ptr_to_elem = ctx.builder.CreateStructGEP(from, idx);
                    auto *const ptr_to_dest_elem = ctx.builder.CreateStructGEP(to, idx);
                    create_deep_copy(ptr_to_elem, ptr_to_dest_elem);
                }

            } else if (auto *const array_type = llvm::dyn_cast<llvm::ArrayType>(aggregate_type)) {
                auto *const elem_type = array_type->getArrayElementType();

                if (elem_type->isPointerTy()) {
                    for (uint64_t const idx : irange(uint64_t{0u}, array_type->getNumElements())) {
                        auto *const ptr_to_elem = ctx.builder.CreateConstInBoundsGEP2_32(from, 0u, idx);
                        auto *const ptr_to_dest_elem = ctx.builder.CreateConstInBoundsGEP2_32(to, 0u, idx);
                        create_deep_copy(ptr_to_elem, ptr_to_dest_elem);
                    }
                }

            } else {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }

        } else if ((llvm::isa<llvm::AllocaInst>(from) || llvm::isa<llvm::GetElementPtrInst>(from)) &&
                   (pointer_depth(from_type) == pointer_depth(to->getType()))) {
            // Note:
            // When the src value should be loaded before storing to dest,
            // pointer depth of src value must equal to one of dest value.
            ctx.builder.CreateStore(ctx.builder.CreateLoad(from), to);
        } else {
            ctx.builder.CreateStore(from, to);
        }
    }

};

class new_alloc_helper {
    context &ctx;
    type_ir_emitter &type_emitter;

public:

    new_alloc_helper(context &c, type_ir_emitter &e)
        : ctx(c), type_emitter(e)
    {}

    template<class String = char const* const>
    llvm::AllocaInst *create_alloca(type::type const& t, String const& name = "", llvm::Value *const array_size = nullptr)
    {
        auto *const ty = type_emitter.emit_alloc_type(t);
        return ctx.builder.CreateAlloca(ty, array_size, name);
    }

    template<class V, class String = char const* const>
    llvm::AllocaInst *alloc_and_deep_copy(V *const from, type::type const& t, String const& name = "", llvm::Value *const array_size = nullptr)
    {
        auto *const allocated = create_alloca(t, name, array_size);
        assert(allocated);

        create_deep_copy(from, allocated, t);

        return allocated;
    }

    template<class V1, class V2>
    void create_deep_copy(V1 *const from, V2 *const to, type::type const& t)
    {
        assert(to->getType()->isPointerTy());

        if (auto const builtin = type::get<type::builtin_type>(t)) {
            if (from->getType()->isPointerTy()) {
                auto const loaded = ctx.builder.CreateLoad(from);
                ctx.builder.CreateStore(loaded, to);
            } else {
                ctx.builder.CreateStore(from, to);
            }
            return;
        }

        assert(from->getType()->isPointerTy());
        auto *const stripped_ty = from->getType()->getPointerElementType();

        ctx.builder.CreateMemCpy(
                to,
                from,
                ctx.data_layout->getTypeAllocSize(stripped_ty),
                ctx.data_layout->getPrefTypeAlignment(stripped_ty)
            );

        // TODO:
        // I should use visitor to visit each type.
        if (auto const tuple_ = type::get<type::tuple_type>(t)) {
            auto const& tuple = *tuple_;
            for (auto const idx : helper::indices(tuple->element_types)) {
                auto const& elem = tuple->element_types[idx];
                if (elem.is_builtin()) {
                    continue;
                }
                auto *const elem_from = ctx.builder.CreateStructGEP(from, idx);
                auto *const elem_to = ctx.builder.CreateStructGEP(to, idx);
                create_deep_copy(elem_from, elem_to, elem);
            }
        } else if (auto const array_ = type::get<type::array_type>(t)) {
            auto const& array = *array_;
            if (array->element_type.is_builtin()) {
                return;
            }
            auto *const array_ty = llvm::dyn_cast<llvm::ArrayType>(stripped_ty);
            assert(array_ty);
            for (uint64_t const idx : helper::indices(array_ty->getNumElements())) {
                auto *const elem_from = ctx.builder.CreateConstInBoundsGEP2_32(from, 0u, idx);
                auto *const elem_to = ctx.builder.CreateConstInBoundsGEP2_32(to, 0u, idx);
                create_deep_copy(elem_from, elem_to, array->element_type);
            }
        } else if (auto const clazz_ = type::get<type::class_type>(t)) {
            auto const& clazz = *clazz_;
            assert(clazz->param_types.empty());
            auto const scope = clazz->ref.lock();
            assert(!scope->is_template());
            for (auto const idx : helper::indices(scope->instance_var_symbols)) {
                auto const& var = scope->instance_var_symbols[idx]->type;
                if (var.is_builtin()) {
                    continue;
                }
                auto *const elem_from = ctx.builder.CreateConstInBoundsGEP2_32(from, 0u, idx);
                auto *const elem_to = ctx.builder.CreateConstInBoundsGEP2_32(to, 0u, idx);
                create_deep_copy(elem_from, elem_to, var);
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }
};

} // namespace builder
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
