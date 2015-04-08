#if !defined DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED

#include "dachs/semantics/type.hpp"

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

class allocation_emitter {
    context &ctx;
    type_ir_emitter &type_emitter;

    using val = llvm::Value *;

    template<class Block>
    val emit_malloc_call(Block *const insert_after, llvm::Type *const elem_ty, val const size_value)
    {
        auto *const intptr_ty = ctx.builder.getIntPtrTy(ctx.data_layout);
        auto *const emitted
            = llvm::CallInst::CreateMalloc(
                    insert_after,
                    intptr_ty,
                    elem_ty,
                    llvm::ConstantInt::get(intptr_ty, ctx.data_layout->getTypeAllocSize(elem_ty)),
                    size_value,
                    nullptr /*malloc func*/,
                    "malloc_call"
                );
        ctx.builder.Insert(emitted);

        assert(emitted->getType() == elem_ty->getPointerTo());

        return emitted;
    }

public:

    allocation_emitter(context &c, type_ir_emitter &e) noexcept
        : ctx(c), type_emitter(e)
    {}

    val emit_malloc(type::type const& elem_type, std::size_t const size)
    {
        auto *const elem_ty = type_emitter.emit_alloc_type(elem_type);
        if (size == 0u) {
            return llvm::ConstantPointerNull::get(elem_ty->getPointerTo());
        } else {
            return emit_malloc_call(
                    ctx.builder.GetInsertBlock(),
                    elem_ty,
                    ctx.builder.getInt32(static_cast<uint32_t>(size))
                );
        }
    }

    val emit_malloc(type::type const& elem_type, val const size_value)
    {
        if (auto *const const_size = llvm::dyn_cast<llvm::ConstantInt>(size_value)) {
            auto const size = const_size->getZExtValue();
            return emit_malloc(elem_type, size);
        }

        auto *const elem_ty = type_emitter.emit_alloc_type(elem_type);

        auto *const parent = ctx.builder.GetInsertBlock()->getParent();
        auto *const then_block = llvm::BasicBlock::Create(ctx.llvm_context, "if.then", parent);
        auto *const else_block = llvm::BasicBlock::Create(ctx.llvm_context, "if.else", parent);
        auto *const merge_block = llvm::BasicBlock::Create(ctx.llvm_context, "if.merge", parent);

        auto *const cond_value = ctx.builder.CreateICmpEQ(size_value, ctx.builder.getInt64(0u));
        ctx.builder.CreateCondBr(cond_value, then_block, else_block);
        ctx.builder.SetInsertPoint(then_block);

        auto *const then_value = llvm::ConstantPointerNull::get(elem_ty->getPointerTo());
        ctx.builder.CreateBr(merge_block);
        ctx.builder.SetInsertPoint(else_block);

        auto *const else_value = emit_malloc_call(
                else_block,
                type_emitter.emit_alloc_type(elem_type),
                size_value
            );
        ctx.builder.CreateBr(merge_block);
        ctx.builder.SetInsertPoint(merge_block);

        auto *const merged = ctx.builder.CreatePHI(elem_ty->getPointerTo(), 2, "if.phi");
        merged->addIncoming(then_value, then_block);
        merged->addIncoming(else_value, else_block);

        return merged;
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_ALLOCATION_EMITTER_HPP_INCLUDED
