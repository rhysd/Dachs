#if !defined DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED

#include <unordered_map>

#include <boost/optional.hpp>

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>

#include "dachs/semantics/symbol.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class variable_table {
    using val = llvm::Value *;
    using table_type = std::unordered_map<symbol::var_symbol, val>;

    table_type register_table;
    table_type alloca_table;

    llvm::IRBuilder<> &builder;

    template<class Map, class T>
    boost::optional<val> lookup_table(Map const& heystack, T const& needle) const noexcept
    {
        auto const result = heystack.find(needle);
        if (result == std::end(heystack)) {
            return boost::none;
        } else {
            return result->second;
        }
    }

public:

    explicit variable_table(llvm::IRBuilder<> &b) noexcept
        : builder(b)
    {}

    val get_ir_for(symbol::var_symbol const& sym) noexcept
    {
        if (auto const maybe_reg_val = lookup_table(register_table, sym)) {
            return *maybe_reg_val;
        }

        if (auto const maybe_alloca_val = lookup_table(alloca_table, sym)) {
            auto const alloca_val = *maybe_alloca_val;
            return builder.CreateLoad(alloca_val);
        }

        return nullptr;
    }

    bool insert(symbol::var_symbol const& key, val const value) noexcept
    {
        return register_table.emplace(key, value).second;
    }

    bool insert(symbol::var_symbol const& key, llvm::AllocaInst *const value) noexcept
    {
        return alloca_table.emplace(key, value).second;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
