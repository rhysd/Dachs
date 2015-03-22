#if !defined DACHS_HELPER_LLVM_HPP_INCLUDED
#define      DACHS_HELPER_LLVM_HPP_INCLUDED

#include <iostream>

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

namespace dachs {
namespace helper {

void dump(llvm::Value const* const v)
{
    v->getType()->dump();
    std::cerr << ": " << std::flush;
    v->dump();
}

void dump(llvm::Type const* const t)
{
    t->dump();
    std::cerr << std::endl;
}

template<class T>
T inspect(T const v)
{
    v->dump();
    return v;
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_LLVM_HPP_INCLUDED
