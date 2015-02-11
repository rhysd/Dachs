#if !defined DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED

#include <unordered_map>
#include <cassert>
#include <iostream>

#include <boost/optional.hpp>

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Function.h>

#include "dachs/semantics/symbol.hpp"
#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

template<class Map, class T>
inline boost::optional<typename Map::mapped_type> lookup_table(Map const& heystack, T const& needle)
{
    auto const result = heystack.find(needle);
    if (result == std::end(heystack)) {
        return boost::none;
    } else {
        return result->second;
    }
}

template<class Map, class T>
inline bool exists_in_table(Map const& heystack, T const& needle)
{
    return heystack.find(needle) != std::end(heystack);
}

template<class T>
inline bool is_aggregate_ptr(T const *const t)
{
    if (!t->isPointerTy()) {
        return false;
    }

    auto *const elem_type = t->getPointerElementType();
    return elem_type->isStructTy() || elem_type->isArrayTy();
}

} // namespace detail

// Note:
// This class will be needless because all of variables have reference semantics.

// Note:
// This class has some tables for registers and allocation instructions.
// However, a table for allocation instructions are wrong.
// It should be for pointer type value because not only alloca instruction 
// but also GEP is pointer type value.

// Note:
//
// Value
// |
// |- Register Value
// |
// |- Alloca Value
//    |
//    |- Aggregate Value (Struct and Array)
//    |- Other Value

class variable_table {
    using val = llvm::Value *;
    using table_type = std::unordered_map<symbol::var_symbol, val>;
    using alloca_table_type = std::unordered_map<symbol::var_symbol, llvm::AllocaInst *>;

    context &ctx;
    table_type register_table;
    alloca_table_type alloca_table;
    alloca_table_type alloca_aggregate_table;
    // table_type global_table;
    // table_type constant_table; // Need?

public:

    explicit variable_table(context &c) noexcept
        : ctx(c)
    {}

    template<class Stream = std::ostream>
    void dump(Stream &out = std::cerr) noexcept
    {
        auto const dump_table
            = [&out](auto const& t)
            {
                for (auto const& row : t) {
                    auto const a = row.first->ast_node.get_weak();
                    std::string location = "location:";
                    if (a.expired()) {
                        location += "UNKNOWN";
                    } else {
                        auto const s = a.lock();
                        location += "line:" + std::to_string(s->line) + ":col:" + std::to_string(s->col);
                    }
                    out << location << ": \"" << row.first->name << "\" ";
                    row.second->dump();
                }
            };
        dump_table(register_table);
        dump_table(alloca_table);
        dump_table(alloca_aggregate_table);
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
            return ctx.builder.CreateStore(v, *maybe_alloca_val);
        }

        if (auto const maybe_allocated_aggregate = detail::lookup_table(alloca_aggregate_table, sym)) {
            auto const& allocated_aggregate = *maybe_allocated_aggregate;
            auto *const t = allocated_aggregate->getAllocatedType();
            ctx.builder.CreateMemCpy(
                    allocated_aggregate,
                    v,
                    ctx.data_layout->getTypeAllocSize(t),
                    ctx.data_layout->getPrefTypeAlignment(t)
            );

            return v;
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
        if (auto const maybe_aggregate_val = detail::lookup_table(alloca_aggregate_table, s)) {
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

        if (auto const maybe_aggregate_val = detail::lookup_table(alloca_aggregate_table, s)) {
            return *maybe_aggregate_val;
        }

        return nullptr;
    }

    val lookup_value_by_name(std::string const& name)
    {
        auto const by_name = [&name](auto const& s){ return s.first->name == name; };

        if (auto const maybe_reg_val = helper::find_if(register_table, by_name)) {
            return maybe_reg_val->second;
        }

        if (auto const maybe_alloca_val = helper::find_if(alloca_table, by_name)) {
            return maybe_alloca_val->second;
        }

        if (auto const maybe_aggregate_val = helper::find_if(alloca_aggregate_table, by_name)) {
            return maybe_aggregate_val->second;
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
        if (detail::is_aggregate_ptr(value->getType())) {
            return alloca_aggregate_table.emplace(key, value).second;
        } else {
            return alloca_table.emplace(key, value).second;
        }
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_VARIABLE_TABLE_HPP_INCLUDED
