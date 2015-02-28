#if !defined DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <cstdint>

#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class builtin_function_emitter {
    llvm::Module *module = nullptr;
    context &c;
    type_ir_emitter &type_emitter;

    // Argument type name -> Function
    using print_func_table_type = std::unordered_map<std::string, llvm::Function *const>;
    using address_of_func_table_type = print_func_table_type;
    print_func_table_type print_func_table;
    print_func_table_type println_func_table;
    llvm::Function *cityhash_func = nullptr;
    llvm::Function *malloc_func = nullptr;
    address_of_func_table_type address_of_func_table;
    llvm::Function *getchar_func = nullptr;

public:

    builtin_function_emitter(decltype(c) &ctx, type_ir_emitter &e)
        : c(ctx), type_emitter(e)
    {}

    void set_module(llvm::Module *m) noexcept
    {
        module = m;
    }

    template<class Table>
    llvm::Function *emit_print_func_prototype(Table &table, std::string const& prefix, type::builtin_type const& arg_type)
    {
        auto const func_itr = table.find(arg_type->name);
        if (func_itr != std::end(table)) {
            return func_itr->second;
        }

        auto const define_func_prototype =
            [&](llvm::Type *arg_type_ir)
            {
                std::vector<llvm::Type *> param_types
                    = {arg_type_ir};
                auto const print_func_type = llvm::FunctionType::get(
                        llvm::StructType::get(c.llvm_context, {})->getPointerTo(),
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
        auto *const target_func = define_func_prototype(type_emitter.emit(arg_type));

        table.emplace(arg_type->name, target_func);
        return target_func;
    }

    llvm::Function *emit_malloc_func()
    {
        if (malloc_func) {
            return malloc_func;
        }

        auto const func_type = llvm::FunctionType::get(
                c.builder.getInt8PtrTy(),
                {c.builder.getInt64Ty()},
                false
            );

        malloc_func
            = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    "__dachs_malloc__",
                    module
                );

        return malloc_func;
    }

    llvm::Value *emit_malloc_call(llvm::Type *const ty)
    {
        auto const size = c.data_layout->getTypeAllocSize(ty);
        if (size == 0u) {
            return llvm::ConstantPointerNull::get(ty->getPointerTo());
        }

        auto *const malloc_func = emit_malloc_func();

        auto *const size_value
            = llvm::ConstantInt::get(
                c.builder.getInt64Ty(),
                size,
                false /*isSigned*/
            );

        auto *const call_inst
            = c.builder.CreateCall(
                    malloc_func,
                    size_value
                );

        return c.builder.CreateBitCast(
                call_inst,
                ty->getPointerTo()
            );
    }

    llvm::Function *emit_cityhash_func()
    {
        if (cityhash_func) {
            return cityhash_func;
        }

        auto const func_type = llvm::FunctionType::get(
                c.builder.getInt64Ty(),
                {c.builder.getInt8PtrTy()},
                false
            );

        cityhash_func
            = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    "__dachs_cityhash__",
                    module
                );

        return cityhash_func;
    }

    // TODO:
    // This is temporary implementation.
    llvm::Function *emit_print_func(type::builtin_type const& arg_type)
    {
        return emit_print_func_prototype(print_func_table, "__dachs_print_", arg_type);
    }

    llvm::Function *emit_println_func(type::builtin_type const& arg_type)
    {
        return emit_print_func_prototype(println_func_table, "__dachs_println_", arg_type);
    }

    llvm::Function *emit_read_cycle_counter_func()
    {
        assert(module);
        return llvm::Intrinsic::getDeclaration(module, llvm::Intrinsic::readcyclecounter);
    }

    llvm::Function *emit_getchar_func()
    {
        if (getchar_func) {
            return getchar_func;
        }

        auto const func_type = llvm::FunctionType::get(
                c.builder.getInt8Ty(),
                false
            );

        getchar_func
            = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    "__dachs_getchar__",
                    module
                );

        return getchar_func;
    }

    llvm::Function *emit_address_of_func(type::type const& arg_type)
    {
        auto *const arg_ty = type_emitter.emit(arg_type);
        if (!arg_ty->isPointerTy()) {
            throw code_generation_error{
                "LLVM IR generator", "\n  Failed to emit builtin function: "
                "argument of __builtin_address_of(" + arg_type.to_string() + ") must be pointer"
            };
        }

        std::string type_str = arg_type.to_string();

        auto const func_itr = address_of_func_table.find(type_str);
        if (func_itr != std::end(address_of_func_table)) {
            return func_itr->second;
        }

        auto *const func_ty = llvm::FunctionType::get(
                c.builder.getInt64Ty(),
                (llvm::Type *[1]){arg_ty},
                false
            );

        auto *const prototype = llvm::Function::Create(
                func_ty,
                llvm::Function::ExternalLinkage,
                "__builtin_address_of",
                module
            );
        prototype->addFnAttr(llvm::Attribute::NoUnwind);
        prototype->addFnAttr(llvm::Attribute::InlineHint);

        auto const arg_value = prototype->arg_begin();
        arg_value->setName("ptr");

        auto const block = llvm::BasicBlock::Create(c.llvm_context, "entry", prototype);
        llvm::ReturnInst::Create(
                c.llvm_context,
                new llvm::PtrToIntInst(arg_value, c.builder.getInt64Ty(), "", block),
                block
            );

        address_of_func_table.emplace(std::move(type_str), prototype);

        return prototype;
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
        } else if (name == "__builtin_read_cycle_counter") {
            return emit_read_cycle_counter_func();
        } else if (name == "__builtin_address_of") {
            return emit_address_of_func(arg_types[0]);
        } else if (name == "__builtin_getchar") {
            assert(arg_types.empty());
            return emit_getchar_func();
        } // else ...

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
