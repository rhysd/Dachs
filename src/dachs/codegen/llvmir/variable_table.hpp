#if !defined DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED

#include <unordered_map>
#include <cassert>

#include <boost/optional.hpp>

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>

#include "dachs/semantics/symbol.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

template<class Map, class T>
boost::optional<llvm::Value *> lookup_table(Map const& heystack, T const& needle)
{
    auto const result = heystack.find(needle);
    if (result == std::end(heystack)) {
        return boost::none;
    } else {
        return result->second;
    }
}

template<class Map, class T>
bool exists_in_table(Map const& heystack, T const& needle)
{
    return heystack.find(needle) != std::end(heystack);
}

} // namespace detail

class variable_table {
    using val = llvm::Value *;
    using table_type = std::unordered_map<symbol::var_symbol, val>;

    table_type register_table;
    table_type alloca_table;
    table_type aggregate_table;
    // table_type global_table;
    // table_type constant_table; // Need?

    llvm::IRBuilder<> &builder;

public:

    explicit variable_table(llvm::IRBuilder<> &b) noexcept
        : builder(b)
    {}

    val emit_ir_to_load(symbol::var_symbol const& sym) noexcept
    {
        if (auto const maybe_reg_val = detail::lookup_table(register_table, sym)) {
            return *maybe_reg_val;
        }

        if (auto const maybe_alloca_val = detail::lookup_table(alloca_table, sym)) {
            return builder.CreateLoad(*maybe_alloca_val, sym->name);
        }

        if (auto const maybe_aggregate_val = detail::lookup_table(aggregate_table, sym)) {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        return nullptr;
    }

    template<class Value>
    val emit_ir_to_store(symbol::var_symbol const& sym, Value const v) noexcept
    {
        // Note:
        // Can't store a value to register
        if (detail::exists_in_table(register_table, sym)) {
            return nullptr;
        }

        if (auto const maybe_alloca_val = detail::lookup_table(alloca_table, sym)) {
            return builder.CreateStore(v, *maybe_alloca_val);
        }

        if (auto const maybe_aggregate_val = detail::lookup_table(aggregate_table, sym)) {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        return nullptr;
    }

    val lookup_register_value(symbol::var_symbol const& s) const noexcept
    {
        if (auto const maybe_reg_val = detail::lookup_table(register_table, s)) {
            return *maybe_reg_val;
        } else {
            return nullptr;
        }
    }

    val lookup_alloca_value(symbol::var_symbol const& s) const noexcept
    {
        if (auto const maybe_alloca_val = detail::lookup_table(alloca_table, s)) {
            return *maybe_alloca_val;
        } else {
            return nullptr;
        }
    }

    val lookup_aggregate_value(symbol::var_symbol const& s) const noexcept
    {
        if (auto const maybe_aggregate_val = detail::lookup_table(aggregate_table, s)) {
            return *maybe_aggregate_val;
        } else {
            return nullptr;
        }
    }

    val lookup_value(symbol::var_symbol const& s) const noexcept
    {
        if (auto const maybe_reg_val = detail::lookup_table(register_table, s)) {
            return *maybe_reg_val;
        }

        if (auto const maybe_alloca_val = detail::lookup_table(alloca_table, s)) {
            return *maybe_alloca_val;
        }

        if (auto const maybe_aggregate_val = detail::lookup_table(aggregate_table, s)) {
            return *maybe_aggregate_val;
        }

        return nullptr;
    }

    bool erase_register_value(symbol::var_symbol const& s) noexcept
    {
        return register_table.erase(s) == 1;
    }

    bool erase_alloca_value(symbol::var_symbol const& s) noexcept
    {
        return alloca_table.erase(s) == 1;
    }

    bool erase_value(symbol::var_symbol const& s) noexcept
    {
        return erase_register_value(s) || erase_alloca_value(s);
    }

    bool insert(symbol::var_symbol const& key, val const value) noexcept
    {
        assert(!detail::exists_in_table(alloca_table, key));
        return register_table.emplace(key, value).second;
    }

    bool insert(symbol::var_symbol const& key, llvm::AllocaInst *const value) noexcept
    {
        assert(!detail::exists_in_table(register_table, key));
        return alloca_table.emplace(key, value).second;
    }

    bool insert(symbol::var_symbol const& key, llvm::StructType *const value) noexcept
    {
        assert(!detail::exiswts_in_table(aggregate_table, key));
        return aggregate_table.emplace(key, value).second;
    }

    bool insert(symbol::var_symbol const& key, llvm::ArrayType *const value) noexcept
    {
        assert(!detail::exiswts_in_table(aggregate_table, key));
        return aggregate_table.emplace(key, value).second;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
