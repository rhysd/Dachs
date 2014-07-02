#include <vector>
#include <string>

#include <llvm/IR/Module.h>

#include "dachs/codegen/llvmir/executable_generator.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

void generate_executable(llvm::Module &target_module, std::string const& file_name, std::vector<std::string> const& linker_options)
{
    // TODO
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs
