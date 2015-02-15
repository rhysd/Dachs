#if !defined DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED

#include <vector>
#include <string>

#include <llvm/IR/Module.h>

#include "dachs/codegen/llvmir/context.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class binary_generator;

std::string generate_executable(std::vector<llvm::Module *> const& modules, std::vector<std::string> const& libdirs, context &ctx, std::string parent = "");
std::vector<std::string> generate_objects(std::vector<llvm::Module *> const& modules, context &ctx, std::string parent = "");

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_EXECUTABLE_GENERATOR_HPP_INCLUDED
