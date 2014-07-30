#if !defined DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED

#include <memory>
#include <algorithm>

#include <boost/range/irange.hpp>
#include <boost/range/adaptor/filtered.hpp>

#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/codegen/llvmir/context.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

using boost::adaptors::filtered;
using boost::irange;

template<class Node>
class basic_ir_builder_helper {
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

    template<class T>
    inline bool is_aggregate_ptr(T const *const t) const noexcept
    {
        if (!t->isPointerTy()) {
            return false;
        }

        auto *const elem_type = t->getPointerElementType();
        return elem_type->isAggregateType();
    }

public:
    basic_ir_builder_helper(node_type const& n, context &c) noexcept
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

    template<class TypeType, class String = char const* const>
    llvm::AllocaInst *create_alloca(TypeType *const type, llvm::Value *const array_size = nullptr, String const& name = "")
    {
        // Note:
        // Absorb the difference between value types and reference types
        return check(
            ctx.builder.CreateAlloca(
                type->isPointerTy() ?
                    type->getPointerElementType()
                  : type
                , array_size
                , name
            )
            , "alloca instruction"
        );
    }

    template<class String = char const* const>
    llvm::AllocaInst *alloc_and_deep_copy(llvm::Value *const from, String const& name = "")
    {
        auto *const allocated
            = check(
                create_alloca(from->getType(), nullptr/*TODO*/, name)
                , "alloc and deep copy"
            );

        create_deep_copy(from, allocated);
        return allocated;
    }

    void create_deep_copy(llvm::Value *const from, llvm::AllocaInst *const to)
    {
        assert(from);
        assert(to);
        auto *const t = from->getType();

        if (is_aggregate_ptr(t)) {
            auto *const aggregate_type = t->getPointerElementType();
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
                    if (!struct_type->getElementType(i)->isPointerTy()) {
                        continue;
                    }

                    auto *const ptr_to_elem = ctx.builder.CreateStructGEP(from, idx);
                    ctx.builder.CreateStore(
                            alloc_and_deep_copy(
                                ctx.builder.CreateLoad(ptr_to_elem)
                            )
                            , ptr_to_elem
                        );
                }
            } else if (auto *const array_type = llvm::dyn_cast<llvm::ArrayType>(aggregate_type)) {
                auto *const elem_type = array_type->getArrayElementType();
                if (elem_type->isPointerTy()) {
                    for (uint64_t const idx
                        : irange((uint64_t)0u, array_type->getNumElements())) {
                        auto *const ptr_to_elem = ctx.builder.CreateConstInBoundsGEP2_32(from, 0u, idx);
                        ctx.builder.CreateStore(
                                alloc_and_deep_copy(
                                    ctx.builder.CreateLoad(ptr_to_elem)
                                )
                                , ptr_to_elem
                            );
                    }
                }
            } else {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }
        } else if (t->isPointerTy()) {
            ctx.builder.CreateStore(ctx.builder.CreateLoad(from), to);
        } else {
            ctx.builder.CreateStore(from, to);
        }
    }

};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
