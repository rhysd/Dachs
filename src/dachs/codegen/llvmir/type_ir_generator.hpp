#if !defined DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>

#include "dachs/semantics/type.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

llvm::Type *generate_type_ir(type::type const& t, llvm::LLVMContext &);

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_TYPE_IR_GENERATOR_HPP_INCLUDED
