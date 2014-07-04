#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

#include <boost/format.hpp>

#include <llvm/IR/Module.h>
#include <llvm/ADT/Triple.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FormattedStream.h>

#include "dachs/codegen/llvmir/executable_generator.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {


namespace detail {
    struct unused {};
} // namespace detail

detail::unused initialize_x86_info_once()
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
            return detail::unused{};
        }();
    return u;
}

std::string generate_executable(llvm::Module &module, std::string const& file_name, std::vector<std::string> const& libdirs)
{
    initialize_x86_info_once();

    auto const dot_pos = file_name.rfind('.');
    if (dot_pos == std::string::npos) {
        throw code_generation_error{"LLVM IR generator", "Invalid file name: Extension is not found."};
    }
    auto const base_name = file_name.substr(0, dot_pos);

    llvm::Triple triple(llvm::sys::getDefaultTargetTriple());

    std::string err;
    llvm::Target const* const target
        = llvm::TargetRegistry::lookupTarget(
            triple.getTriple(),
            err
        );

    if (!target) {
        throw code_generation_error{"LLVM IR generator", boost::format("On looking up target with '%1%': %2%") % triple.getTriple() % err};
    }

    llvm::TargetOptions options;
    auto *const target_machine
        = target->createTargetMachine(
            triple.getTriple(),
            "", // CPU name
            "", // Feature names
            options
        );

    if (!target_machine) {
        throw code_generation_error{"LLVM IR generator", boost::format("Failed to get a target machine for %1%") % triple.getTriple()};
    }

    llvm::PassManager pm;
    pm.add(new llvm::TargetLibraryInfo(triple));

    target_machine->addAnalysisPasses(pm);

    if (llvm::DataLayout const* const layout = target_machine->getDataLayout()) {
        pm.add(new llvm::DataLayout(*layout));
    } else {
        pm.add(new llvm::DataLayout(&module));
    }

    auto const obj_name = base_name + ".o";
    {
        llvm::tool_output_file out{obj_name.c_str(), err, llvm::sys::fs::F_None | llvm::sys::fs::F_Binary};

        llvm::formatted_raw_ostream formatted_os{out.os()};
        if (target_machine->addPassesToEmitFile(pm, formatted_os, llvm::TargetMachine::CGFT_ObjectFile)) {
            throw code_generation_error{"LLVM IR generator", boost::format("Failed to create an object file '%1%': %2%") % obj_name % err};
        }

        pm.run(module);

        // Note:
        // Do not delete object file
        out.keep();
    }

    {
        // TODO: Temporary

        auto const os_type = triple.getOS();
        auto command
            = os_type == llvm::Triple::Darwin
                ? "ld -macosx_version_min 10.9.0 \"" + obj_name + "\" -o \"" + base_name + "\" -lSystem -ldachs"
                : "clang " + obj_name + " -o " + base_name + " -ldachs"; // Fallback...

        for (auto const& lib : libdirs) {
            command += " -L \"" + lib + '"';
        }

        int const cmd_result = std::system(command.c_str());
        if (WEXITSTATUS(cmd_result) != 0) {
            throw code_generation_error{"LLVM IR generator", boost::format("Linker command exited with status %1%. Command was: %2%") % WEXITSTATUS(cmd_result) % command};
        }
    }

    if (std::remove(obj_name.c_str()) != 0) {
        throw code_generation_error{"LLVM IR generator", "Failed to remove an object file: " + obj_name};
    }

    return base_name;
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs
