#if !defined DACHS_HELPER_LLVM_HPP_INCLUDED
#define      DACHS_HELPER_LLVM_HPP_INCLUDED

#include <iostream>
#include <type_traits>

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

namespace dachs {
namespace helper {

extern void* enabler;

template<class V, class String = char const* const, typename std::enable_if<std::is_base_of<llvm::Value, V>::value>::type *& = enabler>
V *dump(V *const v, String const& msg = "")
{
    std::cerr << msg;
    v->getType()->dump();
    std::cerr << ": " << std::flush;
    v->dump();
    return v;
}

template<class T, class String = char const* const, typename std::enable_if<std::is_base_of<llvm::Type, T>::value>::type *& = enabler>
T *dump(T *const t, String const& msg = "")
{
    std::cerr << msg;
    t->dump();
    std::cerr << std::endl;
    return t;
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
