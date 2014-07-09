#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>

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

class binary_generator_base {
    struct unused {};

public:

    binary_generator_base()
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

    virtual ~binary_generator_base()
    {}
};

class binary_generator final : private binary_generator_base {

    std::vector<llvm::Module *> modules;
    llvm::Triple const triple;
    std::string tmp_buffer;
    llvm::Target const* const target;
    llvm::TargetOptions options;
    llvm::TargetMachine *const target_machine;

    std::string get_base_name_from_module(llvm::Module const& module) const
    {
        auto const& file_name = module.getModuleIdentifier();
        auto const dot_pos = file_name.rfind('.');
        if (dot_pos == std::string::npos) {
            throw code_generation_error{"LLVM IR generator", "Invalid file name: Extension is not found."};
        }
        return file_name.substr(0, dot_pos);
    }

    std::string generate_object(llvm::Module &module)
    {
        llvm::PassManager pm;
        pm.add(new llvm::TargetLibraryInfo(triple));
        target_machine->addAnalysisPasses(pm);

        if (llvm::DataLayout const* const layout = target_machine->getDataLayout()) {
            pm.add(new llvm::DataLayout(*layout));
        } else {
            pm.add(new llvm::DataLayout(&module));
        }

        auto const obj_name = get_base_name_from_module(module) + ".o";

        llvm::tool_output_file out{obj_name.c_str(), tmp_buffer, llvm::sys::fs::F_None | llvm::sys::fs::F_Binary};
        out.keep(); // Do not delete object file

        llvm::formatted_raw_ostream formatted_os{out.os()};
        if (target_machine->addPassesToEmitFile(pm, formatted_os, llvm::TargetMachine::CGFT_ObjectFile)) {
            throw code_generation_error{"LLVM IR generator", boost::format("Failed to create an object file '%1%': %2%") % obj_name % tmp_buffer};
        }

        pm.run(module);

        return obj_name;
    }

public:

    binary_generator(decltype(modules) const& ms) noexcept
        : binary_generator_base()
        , modules(ms)
        , triple(llvm::sys::getDefaultTargetTriple())
        , tmp_buffer()
        , target(llvm::TargetRegistry::lookupTarget(triple.getTriple(), tmp_buffer))
        , options()
        , target_machine(target->createTargetMachine(triple.getTriple(), ""/*cpu name*/, ""/*feature*/, options))
    {
        assert(!ms.empty());
    }

    void verify()
    {
        if (!target) {
            throw code_generation_error{"LLVM IR generator", boost::format("On looking up target with '%1%': %2%") % triple.getTriple() % tmp_buffer};
        }

        if (!target_machine) {
            throw code_generation_error{"LLVM IR generator", boost::format("Failed to get a target machine for %1%") % triple.getTriple()};
        }
    }

    std::vector<std::string> generate_objects()
    {
        std::vector<std::string> obj_names;
        for (auto const m : modules) {
            assert(m);
            obj_names.push_back(generate_object(*m));
        }
        return obj_names;
    }

    std::string generate_executable(std::vector<std::string> const& libdirs)
    {
        // TODO: Temporary
        auto const obj_names = generate_objects();
        auto const os_type = triple.getOS();
        auto const objs_string
            = boost::algorithm::join(obj_names, " ");
        auto const executable_name = get_base_name_from_module(*modules[0]);
        auto command
            = os_type == llvm::Triple::Darwin
                ? "ld -macosx_version_min 10.9.0 \"" + objs_string + "\" -o \"" + executable_name + "\" -lSystem -ldachs -L /usr/lib -L /usr/local/lib"
                : "clang " + objs_string + " -o " + executable_name + " -ldachs -L /usr/lib -L /usr/local/lib"; // Fallback...

        for (auto const& lib : libdirs) {
            command += " -L \"" + lib + '"';
        }

        int const cmd_result = std::system(command.c_str());
        if (WEXITSTATUS(cmd_result) != 0) {
            throw code_generation_error{"LLVM IR generator", boost::format("Linker command exited with status %1%. Command was: %2%") % WEXITSTATUS(cmd_result) % command};
        }

        std::vector<std::string> failed_objs;
        for (auto const& o : obj_names) {
            if (std::remove(o.c_str()) != 0) {
                failed_objs.push_back(o);
            }

        }

        if (!failed_objs.empty()) {
            throw code_generation_error{"LLVM IR generator", "Failed to remove some object files: " + boost::algorithm::join(failed_objs, ", ")};
        }

        return executable_name;
    }
};

std::string generate_executable(std::vector<llvm::Module *> const& modules, std::vector<std::string> const& libdirs)
{
    binary_generator generator{modules};
    generator.verify();
    return generator.generate_executable(libdirs);
}

std::vector<std::string> generate_objects(std::vector<llvm::Module *> const& modules)
{
    binary_generator generator{modules};
    generator.verify();
    return generator.generate_objects();
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs
