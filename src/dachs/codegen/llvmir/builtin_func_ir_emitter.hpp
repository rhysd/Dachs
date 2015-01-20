#if !defined DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>

#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>

#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class builtin_function_emitter {
    llvm::Module *module = nullptr;
    llvm::LLVMContext &context;

    // Argument type name -> Function
    using print_func_table_type = std::unordered_map<std::string, llvm::Function *const>;
    print_func_table_type print_func_table;
    print_func_table_type println_func_table;

public:

    explicit builtin_function_emitter(decltype(context) &c)
        : context(c)
    {}

    void set_module(llvm::Module *m) noexcept
    {
        module = m;
    }

    template<class Table>
    llvm::Function *emit_builtin_func_prototype(Table &table, std::string const& prefix, type::builtin_type const& arg_type)
    {
        auto const func_itr = table.find(arg_type->name);
        if (func_itr != std::end(table)) {
            return func_itr->second;
        }

        llvm::Function *target = nullptr;

        auto const define_func_prototype =
            [&](llvm::Type *arg_type_ir)
            {
                std::vector<llvm::Type *> param_types
                    = {arg_type_ir};
                auto const print_func_type = llvm::FunctionType::get(
                        llvm::StructType::get(context, {}),
                        param_types,
                        false
                    );
                return llvm::Function::Create(
                        print_func_type,
                        llvm::Function::ExternalLinkage,
                        prefix + arg_type->name + "__",
                        module
                    );
            };

        assert(module);
        auto const& n = arg_type->name;
        if (n == "string" || n == "symbol") {
            target = define_func_prototype(llvm::Type::getInt8PtrTy(context));
        } else if (n == "int" || n == "uint") {
            target = define_func_prototype(llvm::Type::getInt64Ty(context));
        } else if (n == "float") {
            target = define_func_prototype(llvm::Type::getDoubleTy(context));
        } else if (n == "char") {
            target = define_func_prototype(llvm::Type::getInt8Ty(context));
        } else if (n == "bool") {
            target = define_func_prototype(llvm::Type::getInt1Ty(context));
        }

        table.insert(std::make_pair(arg_type->name, target));
        return target;
    }

    // TODO:
    // This is temporary implementation.
    llvm::Function *emit_print_func(type::builtin_type const& arg_type)
    {
        return emit_builtin_func_prototype(print_func_table, "__dachs_print_", arg_type);
    }

    llvm::Function *emit_println_func(type::builtin_type const& arg_type)
    {
        return emit_builtin_func_prototype(println_func_table, "__dachs_println_", arg_type);
    }

    llvm::Function *emit_read_cycle_counter_func()
    {
        assert(module);
        return llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::readcyclecounter);
    }

    llvm::Function *emit(std::string const& name, std::vector<type::type> const& arg_types)
    {
        if (name == "print") {
            assert(arg_types.size() == 1);
            if (auto const maybe_arg_type = type::get<type::builtin_type>(arg_types[0])) {
                assert(*maybe_arg_type);
                return emit_print_func(*maybe_arg_type);
            }
        } else if (name == "println") {
            assert(arg_types.size() == 1);
            if (auto const maybe_arg_type = type::get<type::builtin_type>(arg_types[0])) {
                assert(*maybe_arg_type);
                return emit_println_func(*maybe_arg_type);
            }
        } else if (name == "read_cycle_counter") {
            return emit_read_cycle_counter_func();
        } // else ...

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
