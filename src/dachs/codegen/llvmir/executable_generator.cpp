#include <vector>
#include <string>
#include <cstdlib>
#include <cstdio>

#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>

#include <llvm/IR/Module.h>
#include <llvm/Target/TargetLibraryInfo.h>
#include <llvm/Pass.h>
#include <llvm/PassManager.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/FormattedStream.h>
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR >= 5)
# include <llvm/Support/FileSystem.h>
#endif

#include "dachs/codegen/llvmir/executable_generator.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class binary_generator final {

    std::vector<llvm::Module *> modules;
    context &ctx;
    opt_level opt;
    llvm::PassManagerBuilder pm_builder;

    std::string get_base_name_from_module(llvm::Module const& module) const
    {
        auto const& file_name = module.getModuleIdentifier();
        auto const dot_pos = file_name.rfind('.');
        if (dot_pos == std::string::npos) {
            throw code_generation_error{"LLVM IR generator", "Invalid file name: Extension is not found."};
        }

        auto const slash_pos = file_name.rfind('/', dot_pos);
        if (slash_pos == std::string::npos || slash_pos+1 >= file_name.size()) {
            return file_name.substr(0, dot_pos);
        }

        return file_name.substr(slash_pos+1, dot_pos - slash_pos - 1);
    }

    template<class PassManager>
    void add_data_layout(PassManager &pm) const
    {
        // Note:
        // This implies that all passes MUST be allocated with 'new'.
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
        pm.add(new llvm::DataLayout(*ctx.data_layout));
#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
        pm.add(new llvm::DataLayoutPass(llvm::DataLayout(*ctx.data_layout)));
#else
# error LLVM: Not supported version.
#endif
    }

    void run_func_passes(llvm::Module &module)
    {
        llvm::FunctionPassManager pm{&module};

        add_data_layout(pm);

        pm_builder.populateFunctionPassManager(pm);

        for (auto &f : module.getFunctionList()) {
            pm.doInitialization();
            pm.run(f);
            pm.doFinalization();
        }
    }

    bool run_module_passes(llvm::Module &module, llvm::formatted_raw_ostream &os)
    {
        ctx.target_machine->setOptLevel(get_target_machine_opt_level());

        llvm::PassManager pm;
        pm_builder.populateModulePassManager(pm);

        ctx.target_machine->addAnalysisPasses(pm);
        add_data_layout(pm);

        if (ctx.target_machine->addPassesToEmitFile(pm, os, llvm::TargetMachine::CGFT_ObjectFile)) {
            return false;
        }

        pm.run(module);

        return true;
    }

    template<class String>
    std::string generate_object(llvm::Module &module, String const parent_dir_path)
    {
        run_func_passes(module);

        auto const obj_name = parent_dir_path + get_base_name_from_module(module) + ".o";

        std::string buffer;
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
        llvm::tool_output_file out{obj_name.c_str(), buffer, llvm::sys::fs::F_None | llvm::sys::fs::F_Binary};
#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
        llvm::tool_output_file out{obj_name.c_str(), buffer, llvm::sys::fs::F_None};
#else
# error LLVM: Not supported version.
#endif
        out.keep(); // Do not delete object file
        llvm::formatted_raw_ostream formatted_os{out.os()};
        if (!run_module_passes(module, formatted_os)) {
            throw code_generation_error{"LLVM IR generator", boost::format("Failed to create an object file '%1%': %2%") % obj_name % buffer};
        }

        return obj_name;
    }

    llvm::CodeGenOpt::Level get_target_machine_opt_level() const
    {
        switch (opt) {
        case opt_level::release:
            return llvm::CodeGenOpt::Aggressive;
        case opt_level::debug:
            return llvm::CodeGenOpt::None;
        case opt_level::none:
        default:
            return llvm::CodeGenOpt::Default;
        }
    }

public:

    binary_generator(decltype(modules) const& ms, context &c, opt_level const o = opt_level::none)
        : modules(ms), ctx(c), opt(o), pm_builder()
    {
        assert(!ms.empty());

        switch (opt) {
        case opt_level::release:
            pm_builder.OptLevel = 3u;
            break;
        case opt_level::debug:
            pm_builder.OptLevel = 0u;
            break;
        case opt_level::none:
        default:
            pm_builder.OptLevel = 2u;
            break;
        }
        pm_builder.SizeLevel = 0u;
        pm_builder.LibraryInfo = new llvm::TargetLibraryInfo(ctx.triple);

        switch (opt) {
        case opt_level::release:
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
            // Note:
            // 225 is a threshold used for -O3
            pm_builder.Inliner = llvm::createFunctionInliningPass(275);
            break;
#endif

        case opt_level::none:
#if (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 4)
            // Note:
            // 225 is a threshold used for -O2
            pm_builder.Inliner = llvm::createFunctionInliningPass(225);
#elif (LLVM_VERSION_MAJOR == 3 && LLVM_VERSION_MINOR <= 5)
            pm_builder.Inliner = llvm::createFunctionInliningPass(pm_builder.OptLevel, pm_builder.SizeLevel);
#else
# error LLVM: Not supported version.
#endif

            break;
        case opt_level::debug:
        default:
            break;
        }
    }

    template<class String>
    std::vector<std::string> generate_objects(String const parent_dir_path)
    {
        std::vector<std::string> obj_names;
        for (auto const m : modules) {
            assert(m);
            obj_names.push_back(generate_object(*m, parent_dir_path));
        }
        return obj_names;
    }

    template<class String>
    std::string generate_executable(std::vector<std::string> const& libdirs, String const parent_dir_path)
    {
        // TODO: Temporary
        auto const obj_names = generate_objects(parent_dir_path);
        auto const os_type = ctx.triple.getOS();
        auto const objs_string
            = boost::algorithm::join(obj_names, " ");
        auto const executable_name = parent_dir_path + get_base_name_from_module(*modules[0]);
        auto command
            = os_type == llvm::Triple::Darwin
                ? "ld -macosx_version_min 10.9.0 \"" + objs_string + "\" -o \"" + executable_name + "\" -lSystem -ldachs-runtime -L /usr/lib -L /usr/local/lib -L " DACHS_INSTALL_PREFIX "/lib"
                : (DACHS_CXX_COMPILER " ") + objs_string + " -o " + executable_name + " -ldachs-runtime -L /usr/lib -L /usr/local/lib -L " DACHS_INSTALL_PREFIX "/lib"; // Fallback...

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

std::string generate_executable(
        std::vector<llvm::Module *> const& modules,
        std::vector<std::string> const& libdirs,
        context &ctx,
        opt_level const opt,
        std::string parent)
{
    binary_generator generator{modules, ctx, opt};
    return generator.generate_executable(libdirs, std::move(parent));
}

std::vector<std::string> generate_objects(
        std::vector<llvm::Module *> const& modules,
        context &ctx,
        opt_level const opt,
        std::string parent)
{
    binary_generator generator{modules, ctx, opt};
    return generator.generate_objects(std::move(parent));
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs
