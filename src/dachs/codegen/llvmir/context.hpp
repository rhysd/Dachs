#if !defined DACHS_CODEGEN_LLVMIR_CONTEXT_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_CONTEXT_HPP_INCLUDED

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/ADT/Triple.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Target/TargetMachine.h>

#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class context_base {
    struct unused {};

public:

    context_base()
    {
        // Initialize once
        static auto const u
            = []
            {
                LLVMInitializeX86TargetInfo();
                LLVMInitializeX86Target();
                LLVMInitializeX86TargetMC();
                LLVMInitializeX86AsmPrinter();
                LLVMInitializeX86AsmParser();
                return unused{};
            }();
        (void)u;
    }

    virtual ~context_base()
    {}
};

class context final : private context_base {

    std::string tmp_buffer;

public:

    llvm::Triple const triple;
    llvm::Target const* const target;
    llvm::TargetOptions options;
    llvm::TargetMachine *const target_machine;
    llvm::DataLayout const* const data_layout;
    llvm::LLVMContext &llvm_context;
    llvm::IRBuilder<> builder;

    context(
        llvm::Triple const triple,
        llvm::Target const* const target,
        llvm::TargetOptions options,
        llvm::TargetMachine *const target_machine,
        llvm::DataLayout const* const data_layout,
        llvm::LLVMContext &llvm_context
    ) noexcept
        : context_base()
        , triple(triple)
        , target(target)
        , options(options)
        , target_machine(target_machine)
        , data_layout(data_layout)
        , llvm_context(llvm_context)
        , builder(llvm_context)
    {}

    context()
        : context_base()
        , tmp_buffer()
        , triple(llvm::sys::getDefaultTargetTriple())
        , target(llvm::TargetRegistry::lookupTarget(triple.getTriple(), tmp_buffer))
        , options()
        , target_machine(target->createTargetMachine(triple.getTriple(), ""/*cpu name*/, ""/*feature*/, options))
        , data_layout(target_machine->getDataLayout())
        , llvm_context(llvm::getGlobalContext())
        , builder(llvm_context)
    {
        if (!target) {
            throw code_generation_error{"LLVM IR generator", boost::format("On looking up target with '%1%': %2%") % triple.getTriple() % tmp_buffer};
        }

        if (!target_machine) {
            throw code_generation_error{"LLVM IR generator", boost::format("Failed to get a target machine for %1%") % triple.getTriple()};
        }

        assert(target);
        assert(target_machine);
        assert(data_layout);
    }

    context(context const&) = delete;
    context &operator=(context const&) = delete;
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_CONTEXT_HPP_INCLUDED
