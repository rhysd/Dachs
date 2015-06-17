#if !defined DACHS_HELPER_LLVM_HPP_INCLUDED
#define      DACHS_HELPER_LLVM_HPP_INCLUDED

#include <iostream>
#include <type_traits>

#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>

#include "dachs/helper/colorizer.hpp"

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

template<class Table>
Table const& dump_table(Table const& table, std::string const& name = "", colorizer const c = helper::colorizer{})
{
    std::cerr << c.cyan("# " + name + " table ") << "(size: " << table.size() << ')' << std::endl;
    for (auto const& entry : table) {
        std::cerr << "  " << c.yellow(entry.first->to_string()) << " -> " << std::flush;
        entry.second->dump();
        std::cerr << std::endl;
    }
    std::cerr << std::endl;

    return table;
};

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_LLVM_HPP_INCLUDED
