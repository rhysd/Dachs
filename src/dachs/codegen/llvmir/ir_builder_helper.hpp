#if !defined DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED

#include <memory>
#include <algorithm>

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>

#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

template<class Node>
class basic_ir_builder_helper {
public:
    using builder_type = llvm::IRBuilder<>;
    using node_type = std::shared_ptr<Node>;
    using parent_ptr_type = decltype(std::declval<builder_type>().GetInsertBlock()->getParent());

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
    basic_ir_builder_helper(node_type const& n, builder_type &b, llvm::LLVMContext &c) noexcept
        : node(n), builder(b), context(c), parent(b.GetInsertBlock()->getParent())
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

        if (!builder.GetInsertBlock()->getTerminator()) {
            br = check(builder.CreateBr(dest), "branch instruction");
        }

        if (next) {
            builder.SetInsertPoint(next);
        }
        return br;
    }

    void append_block(llvm::BasicBlock *const b)
    {
        if (!parent) {
            error("No parent found");
        }

        auto *current_block = builder.GetInsertBlock();
        parent->getBasicBlockList().insertAfter(current_block, b);
        builder.SetInsertPoint(b);
    }

    void append_block(llvm::BasicBlock *const b, llvm::BasicBlock *const next)
    {
        if (!parent) {
            error("No parent found");
        }

        auto *const current_block = builder.GetInsertBlock();
        parent->getBasicBlockList().insertAfter(current_block, b);
        if (next) {
            builder.SetInsertPoint(next);
        }
    }

    llvm::BranchInst *create_br(llvm::BasicBlock *const b)
    {
        auto const br = check(builder.CreateBr(b), "branch instruction");
        builder.SetInsertPoint(b);
        return br;
    }

    llvm::BranchInst *create_br(llvm::BasicBlock *const b, llvm::BasicBlock *const next)
    {
        auto const br = check(builder.CreateBr(b), "branch instruction");
        if (next) {
            builder.SetInsertPoint(next);
        }
        return br;
    }

    llvm::BranchInst *create_cond_br(llvm::Value *const cond_val, llvm::BasicBlock *const if_true, llvm::BasicBlock *const if_false, llvm::BasicBlock *const next)
    {
        auto const cond_br = check(builder.CreateCondBr(cond_val, if_true, if_false), "condition branch");
        if (next) {
            builder.SetInsertPoint(next);
        }
        return cond_br;
    }

    llvm::BranchInst *create_cond_br(llvm::Value *const cond_val, llvm::BasicBlock *const if_true, llvm::BasicBlock *const if_false)
    {
        auto const cond_br = check(builder.CreateCondBr(cond_val, if_true, if_false), "condition branch");
        builder.SetInsertPoint(if_true);
        return cond_br;
    }

    template<class String>
    llvm::BasicBlock *create_block_for_parent(String const& name = "", bool const move_to_the_block = false)
    {
        if (!parent) {
            error("No parent found");
        }

        auto const the_block = check(llvm::BasicBlock::Create(context, name, parent), "basic block");
        if (move_to_the_block) {
            builder.SetInsertPoint(the_block);
        }
        return the_block;
    }

    template<class String>
    auto create_block(String const& name = "", bool const move_to_the_block = false) const
    {
        auto const the_block = check(llvm::BasicBlock::Create(context, name), "basic block");
        if (move_to_the_block) {
            builder.SetInsertPoint(the_block);
        }
        return the_block;
    }

private:
    node_type const& node;
    builder_type &builder;
    llvm::LLVMContext &context;
    parent_ptr_type const parent;
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
