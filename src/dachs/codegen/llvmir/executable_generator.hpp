#if !defined DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED

#include <vector>
#include <string>

#include <llvm/IR/Module.h>

namespace dachs {
namespace codegen {
namespace llvmir {

void generate_executable(llvm::Module &target_module, std::string const& file_name, std::vector<std::string> const& linker_options);

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED
