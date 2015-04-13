#if !defined DACHS_CODEGEN_LLVMIR_GC_ALLOC_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_GC_ALLOC_EMITTER_HPP_INCLUDED

#include <cassert>
#include <initializer_list>
#include <unordered_map>

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>

#include "dachs/semantics/type.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

// Note:
// Should I define our original malloc() and realloc() in runtime?

class gc_alloc_emitter {
    context &ctx;
    type_ir_emitter &type_emitter;
    llvm::Module &module;
    std::unordered_map<std::string, llvm::Function *> func_table;

    using val = llvm::Value *;

    template<class String>
    llvm::Function *create_func(String const& name, llvm::Type *const ret_ty, std::initializer_list<llvm::Type *> const& arg_tys)
    {
        auto const itr = func_table.find(name);
        if (itr != std::end(func_table)) {
            return itr->second;
        }

        auto *const func_ty = llvm::FunctionType::get(
                ret_ty,
                arg_tys,
                false
            );

        auto *const func = llvm::Function::Create(
                func_ty,
                llvm::Function::ExternalLinkage,
                name,
                &module
            );

        func->addFnAttr(llvm::Attribute::NoUnwind);
        func->addFnAttr(llvm::Attribute::NoAlias);

        func_table.emplace(name, func);

        return func;
    }

    llvm::Function *create_malloc_func()
    {
        return create_func(
                "GC_malloc",
                ctx.builder.getInt8PtrTy(),
                {ctx.builder.getIntPtrTy(ctx.data_layout)}
            );
    }

    llvm::Function *create_realloc_func()
    {
        return create_func(
                "GC_realloc",
                ctx.builder.getInt8PtrTy(),
                {
                    ctx.builder.getInt8PtrTy(),
                    ctx.builder.getIntPtrTy(ctx.data_layout)
                }
            );
    }

    llvm::Function *create_gc_init_func()
    {
        return create_func(
                "GC_init",
                ctx.builder.getVoidTy(),
                {}
            );
    }

    llvm::Function *create_free_func()
    {
        return create_func(
                "GC_free",
                ctx.builder.getVoidTy(),
                {ctx.builder.getInt8PtrTy()}
            );
    }

    template<class String>
    val create_malloc_call(llvm::BasicBlock *const insert_end, llvm::Type *const elem_ty, val const size_value, String const& name)
    {
        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);
        auto *const emitted
            = llvm::CallInst::CreateMalloc(
                    insert_end,
                    intptr_ty,
                    elem_ty,
                    llvm::ConstantInt::get(intptr_ty, ctx.data_layout->getTypeAllocSize(elem_ty)),
                    size_value,
                    create_malloc_func(),
                    "malloc.call"
                );
        ctx.builder.Insert(emitted);

        assert(emitted->getType() == elem_ty->getPointerTo());

        emitted->setName(name);

        return emitted;
    }

    val create_bit_cast(val const from_val, llvm::Type *const to_ty, llvm::BasicBlock *const insert_end) const
    {
        auto const ty = from_val->getType();
        assert(ty->isPointerTy() && to_ty->isPointerTy());

        if (ty == to_ty) {
            return from_val;
        }

        return new llvm::BitCastInst(from_val, to_ty, "", insert_end);
    }

    val create_realloc_call(llvm::BasicBlock *const insert_end, val const ptr_value, val const size_value)
    {
        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);
        auto *const ptr_ty = ptr_value->getType();
        auto *const elem_ty = ptr_ty->getPointerElementType();
        assert(elem_ty);

        auto const elem_size = ctx.data_layout->getTypeAllocSize(elem_ty);
        auto *const elem_size_value = llvm::ConstantInt::get(intptr_ty, elem_size);

        val const casted_ptr = create_bit_cast(ptr_value, ctx.builder.getInt8PtrTy(), insert_end);

        auto const new_size_value
            = llvm::BinaryOperator::CreateMul(size_value, elem_size_value, "newsize");
        insert_end->getInstList().push_back(new_size_value);

        auto *const reallocated
            = llvm::CallInst::Create(
                    create_realloc_func(),
                    {
                        casted_ptr,
                        new_size_value
                    },
                    "realloccall"
                );

        insert_end->getInstList().push_back(reallocated);

        return create_bit_cast(reallocated, ptr_ty, insert_end);
    }

    template<class AllocEmitter>
    val emit_null_on_zero_otherwise(val const size_value, AllocEmitter unless_zero)
    {
        assert(!llvm::isa<llvm::Constant>(size_value));

        auto *const zero_block = ctx.builder.GetInsertBlock();
        auto *const parent = zero_block->getParent();
        auto *const nonzero_block = llvm::BasicBlock::Create(ctx.llvm_context, "alloc.nonzero", parent);
        auto *const merge_block = llvm::BasicBlock::Create(ctx.llvm_context, "alloc.merge", parent);

        auto *const cond_value = ctx.builder.CreateICmpEQ(size_value, llvm::ConstantInt::get(ctx.builder.getIntPtrTy(ctx.data_layout), 0u));
        ctx.builder.CreateCondBr(cond_value, merge_block, nonzero_block);

        ctx.builder.SetInsertPoint(nonzero_block);
        auto *const nonnull_value = unless_zero(nonzero_block);
        ctx.builder.CreateBr(merge_block);

        ctx.builder.SetInsertPoint(merge_block);

        auto *const ptr_ty = llvm::dyn_cast<llvm::PointerType>(nonnull_value->getType());
        assert(ptr_ty);

        auto *const merged = ctx.builder.CreatePHI(ptr_ty, 2, "alloc.phi");
        merged->addIncoming(llvm::ConstantPointerNull::get(ptr_ty), zero_block);
        merged->addIncoming(nonnull_value, nonzero_block);

        return merged;
    }

public:

    gc_alloc_emitter(context &c, type_ir_emitter &e, llvm::Module &m) noexcept
        : ctx(c), type_emitter(e), module(m)
    {}

    template<class String = char const*>
    val emit_malloc(type::type const& elem_type, std::size_t const array_size, String const& name = "")
    {
        auto *const elem_ty = type_emitter.emit_alloc_type(elem_type);
        if (array_size == 0u) {
            return llvm::ConstantPointerNull::get(elem_ty->getPointerTo());
        } else {
            return create_malloc_call(
                    ctx.builder.GetInsertBlock(),
                    elem_ty,
                    llvm::ConstantInt::get(ctx.builder.getIntPtrTy(ctx.data_layout), array_size),
                    name
                );
        }
    }

    template<class String = char const*>
    val emit_malloc(type::type const& elem_type, val size_value, String const& name = "")
    {
        if (auto *const const_size = llvm::dyn_cast<llvm::ConstantInt>(size_value)) {
            // Note:
            // Although it optimizes and removes the branch, I do that by my hand to be sure
            return emit_malloc(elem_type, const_size->getZExtValue(), name);
        }

        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);

        if (size_value->getType() == ctx.builder.getInt64Ty()) {
            size_value = ctx.builder.CreateTrunc(size_value, intptr_ty);
        }

        assert(size_value->getType() == intptr_ty);

        return emit_null_on_zero_otherwise(
                size_value,
                [&name, &elem_type, size_value, this](auto const else_block)
                {
                    return create_malloc_call(
                        else_block,
                        type_emitter.emit_alloc_type(elem_type),
                        size_value,
                        name
                    );
                }
            );
    }

    template<class String = char const*>
    val emit_malloc(type::type const& elem_type, String const& name = "")
    {
        return emit_malloc(elem_type, 1u, name);
    }

    template<class String = char const*>
    val emit_alloc(type::type const& elem_type, String const& name = "")
    {
        // Note:
        // Primitive types are treated by value.  No need to allocate them in heap.

        if (elem_type.is_aggregate()) {
            return emit_malloc(elem_type, name);
        } else {
            return ctx.builder.CreateAlloca(type_emitter.emit_alloc_type(elem_type), nullptr, name);
        }
    }

    val emit_realloc(val const ptr_value, std::size_t const array_size)
    {
        auto const ptr_ty = llvm::dyn_cast<llvm::PointerType>(ptr_value->getType());
        assert(ptr_ty);

        if (array_size == 0u) {
            return llvm::ConstantPointerNull::get(ptr_ty);
        } else {
            return create_realloc_call(
                    ctx.builder.GetInsertBlock(),
                    ptr_value,
                    llvm::ConstantInt::get(ctx.builder.getIntPtrTy(ctx.data_layout), array_size)
                );
        }
    }

    val emit_realloc(val const ptr_value, val const size_value)
    {
        if (auto *const const_size = llvm::dyn_cast<llvm::ConstantInt>(size_value)) {
            // Note:
            // Although it optimizes and removes the branch, I do that by my hand to be sure
            return emit_realloc(ptr_value, const_size->getZExtValue());
        }

        return emit_null_on_zero_otherwise(
                size_value,
                [ptr_value, size_value, this](auto const else_block)
                {
                    return create_realloc_call(
                        else_block,
                        ptr_value,
                        size_value
                    );
                }
            );
    }

    val emit_init()
    {
        auto *const gc_init = create_gc_init_func();
        return ctx.builder.CreateCall(gc_init);
    }

    val emit_free(val const ptr_value)
    {
        auto *const void_ptr_value = create_bit_cast(ptr_value, ctx.builder.getInt8PtrTy(), ctx.builder.GetInsertBlock());

        return ctx.builder.CreateCall(
                create_free_func(),
                void_ptr_value
            );
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_GC_ALLOC_EMITTER_HPP_INCLUDED
