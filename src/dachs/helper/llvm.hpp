#if !defined DACHS_HELPER_LLVM_HPP_INCLUDED
#define      DACHS_HELPER_LLVM_HPP_INCLUDED

#include <iostream>

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

namespace dachs {
namespace helper {

void dump(llvm::Value const* const v) noexcept
{
    v->getType();
    std::cout << ": " << std::flush;
    v->dump();
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_LLVM_HPP_INCLUDED
