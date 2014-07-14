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

template<class Node, class... BuilderArgs>
class basic_ir_builder_helper {
public:
    using builder_type = llvm::ir_builder<BuilderArgs...>;
    using node_type = std::shared_ptr<Node>;
    using parent_ptr_type = decltype(std::declval<builder_type>().getInsertBlock()->getParent());

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
        : node(n), builder(b), context(c), parent(b->getInsertBlock()->getParent())
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
    auto create_br(llvm::BasicBlock *const dest, llvm::BasicBlock *const next = nullptr)
        -> decltype(builder.CreateBr(dest))
    {
        if (builder->getInsertBlock()->getTerminator()) {
            return nullptr;
        }

        auto const br = check(builder.CreateBr(dest), "branch instruction");
        if (!next) {
            builder.SetInsertPoint(next);
        }
    }

    void append_block(llvm::BasicBlock *const b)
    {
        if (!parent) {
            error("No parent found");
        }

        // Note:
        // Search the block from the end of the list because,
        // the block often is at the end of the list.
        auto *const current_block = builder->getInsertBlock();
        auto &block_list = parent->getBasicBlockList();
        auto const pos = std::find(block_list.rbegin(), block_list.rend(), current_block);
        assert(pos != block_list.rend());
        block_list.insert(pos, b);
    }

    auto create_cond_br(llvm::Value *const cond_val, llvm::BasicBlock *const if_true, llvm::BasicBlock *const if_false, llvm::BasicBlock *const next = if_true)
    {
        auto const cond_br = check(builder.CreateCondBr(cond_val, if_true, if_false), "condition branch");
        if (next) {
            builder.SetInsertPoint(next);
        }
        return cond_br;
    }

    template<class String>
    auto append_block(String const& name = "", bool const move_to_the_block = false)
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
    auto create_block(String const& name = "") const
    {
        return check(llvm::BasicBlock::Create(context, name), "basic block");
    }

private:
    node_type const& node;
    builder_type &builder;
    llvm::LLVMContext &context;
    parent_ptr_type const parent;
};

template<class Node>
using ir_builder_helper = basic_ir_builder_helper<Node>;

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_BUILDER_HELPER_HPP_INCLUDED
