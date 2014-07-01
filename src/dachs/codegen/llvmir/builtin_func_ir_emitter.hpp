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

public:

    explicit builtin_function_emitter(decltype(context) &c)
        : context(c)
    {}

    void set_module(llvm::Module *m) noexcept
    {
        module = m;
    }

    llvm::Function *emit_print_func(type::builtin_type const& arg_type)
    {
        auto const func_itr = print_func_table.find(arg_type->name);
        if (func_itr != std::end(print_func_table)) {
            return func_itr->second;
        }

        llvm::Function *target = nullptr;

        auto const define_print_func =
            [&](llvm::Type *arg_type_ir)
            {
                std::vector<llvm::Type *> param_types
                    = {arg_type_ir};
                auto const print_func_type = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        param_types,
                        false
                    );
                return llvm::Function::Create(
                        print_func_type,
                        llvm::Function::ExternalLinkage,
                        "dachs_print_" + arg_type->name + "__",
                        module
                    );
            };

        assert(module);
        if (arg_type->name == "string") {
            target = define_print_func(llvm::Type::getInt8PtrTy(context));
        } // TODO: else ...

        print_func_table.insert(std::make_pair(arg_type->name, target));
        return target;
    }

    llvm::Function *emit(std::string const& name, std::vector<type::type> const& arg_types)
    {
        if (name == "print") {
            auto const maybe_arg_type = type::get<type::builtin_type>(arg_types[0]);
            if (auto const& arg_type = *maybe_arg_type) {
                return emit_print_func(arg_type);
            }
        } // else ...

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
