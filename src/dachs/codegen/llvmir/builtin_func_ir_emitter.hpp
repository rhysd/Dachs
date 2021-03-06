#if !defined DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED

#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <utility>
#include <initializer_list>
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
#include "dachs/codegen/llvmir/gc_alloc_emitter.hpp"
#include "dachs/codegen/llvmir/ir_builder_helper.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

class builtin_function_emitter {
    llvm::Module &module;
    context &c;
    type_ir_emitter &type_emitter;
    detail::gc_alloc_emitter &gc_emitter;
    builder::inst_emit_helper &inst_emitter;

    // Argument type name -> Function
    using func_table_type = std::unordered_map<std::string, llvm::Function *const>;
    std::unordered_map<std::string, func_table_type> print_func_tables;
    llvm::Function *gen_symbol_func = nullptr;
    func_table_type address_of_func_table;
    llvm::Function *getchar_func = nullptr;
    std::array<llvm::Function *, 2> fatal_funcs = {{nullptr, nullptr}};
    func_table_type is_null_func_table;
    func_table_type realloc_func_table;
    func_table_type free_func_table;
    llvm::Function *enable_gc_func = nullptr;
    llvm::Function *disable_gc_func = nullptr;
    llvm::Function *gc_disabled_func = nullptr;

    template<class String>
    llvm::Function *create_func_prototype(String const& name, llvm::Type *const ret_ty, std::initializer_list<llvm::Type *> const& arg_tys)
    {
        auto *const func_ty = llvm::FunctionType::get(
                ret_ty,
                arg_tys,
                false
            );

        auto *const func = llvm::Function::Create(
                func_ty,
                llvm::Function::ExternalLinkage,
                name,
                &module
            );

        func->addFnAttr(llvm::Attribute::NoUnwind);

        return func;
    }

    template<class String>
    llvm::Function *create_cached_func_prototype(llvm::Function *&func, String const& name, llvm::Type *const ret_ty, std::initializer_list<llvm::Type *> const& arg_tys)
    {
        if (func) {
            return func;
        }

        func = create_func_prototype(
                name,
                ret_ty,
                arg_tys
            );

        return func;
    }

public:

    builtin_function_emitter(
            llvm::Module &m,
            decltype(c) &ctx,
            type_ir_emitter &te,
            detail::gc_alloc_emitter &ge,
            builder::inst_emit_helper &ie
        ) noexcept
        : module(m)
        , c(ctx)
        , type_emitter(te)
        , gc_emitter(ge)
        , inst_emitter(ie)
    {}

    template<class Table, class Type>
    llvm::Function *emit_print_func_prototype(Table &&table, std::string const& func_name, Type const& arg_type)
    {
        auto const func_itr = table.find(func_name);
        if (func_itr != std::end(table)) {
            return func_itr->second;
        }

        // XXX:
        // Return type mismatches.
        // print() in Dachs returns () but print() in runtime returns void
        auto *const target_func = create_func_prototype(
                func_name,
                llvm::StructType::get(c.llvm_context, {})->getPointerTo(),
                {type_emitter.emit(arg_type)}
            );

        table.emplace(func_name, target_func);
        return target_func;
    }

    llvm::Function *emit_gen_symbol_func()
    {
        return create_cached_func_prototype(
                gen_symbol_func,
                "__dachs_gen_symbol__",
                c.builder.getInt64Ty(),
                {
                    c.builder.getInt8PtrTy(),
                    c.builder.getInt64Ty()
                }
            );
    }

    llvm::Function *emit_is_null_func(type::type const& t)
    {
        auto const ptr = type::get<type::pointer_type>(t);
        assert(ptr);

        std::string const type_str = (*ptr)->pointee_type.to_string();

        auto const func_itr = is_null_func_table.find(type_str);
        if (func_itr != std::end(is_null_func_table)) {
            return func_itr->second;
        }

        auto *const prototype = create_func_prototype(
                "dachs.null?",
                c.builder.getInt1Ty(),
                {type_emitter.emit(*ptr)}
            );

        prototype->addFnAttr(llvm::Attribute::AlwaysInline);

        auto const arg_value = prototype->arg_begin();
        arg_value->setName("ptr");
        auto const block = llvm::BasicBlock::Create(c.llvm_context, "entry", prototype);

        auto *const arg_ty = llvm::dyn_cast<llvm::PointerType>(arg_value->getType());
        assert(arg_ty);

        llvm::ReturnInst::Create(
                c.llvm_context,
                new llvm::ICmpInst(
                    *block,
                    llvm::CmpInst::Predicate::ICMP_EQ,
                    llvm::ConstantPointerNull::get(arg_ty),
                    arg_value,
                    "null_check"
                ),
                block
            );

        is_null_func_table.emplace(type_str, prototype);

        return prototype;
    }

    std::string make_print_func_name(std::string const& name, std::string const& arg_name)
    {
        return "__dachs_" + name + "_" + arg_name + "__";
    }

    llvm::Function *emit_print_func(std::string const& name, type::builtin_type const& arg_type)
    {
        return emit_print_func_prototype(
                print_func_tables[name],
                make_print_func_name(name, arg_type->to_string()),
                arg_type
            );
    }

    llvm::Function *emit_print_func(std::string const& name, type::pointer_type const& arg_type)
    {
        if (arg_type->pointee_type.is_builtin("char")) {
            return emit_print_func_prototype(
                    print_func_tables[name],
                    make_print_func_name(name, "string"),
                    arg_type
                );
        } else {
            return nullptr;
        }
    }

    llvm::Function *emit_read_cycle_counter_func()
    {
        return llvm::Intrinsic::getDeclaration(&module, llvm::Intrinsic::readcyclecounter);
    }

    llvm::Function *emit_getchar_func()
    {
        return create_cached_func_prototype(
                getchar_func,
                "__dachs_getchar__",
                c.builder.getInt8Ty(),
                {}
            );
    }

    llvm::Function *emit_address_of_func(type::type const& arg_type)
    {
        auto *const arg_ty = type_emitter.emit(arg_type);
        if (!arg_ty->isPointerTy()) {
            throw code_generation_error{
                "LLVM IR generator", "\n  Failed to emit builtin function: "
                "Argument of __builtin_address_of(" + arg_type.to_string() + ") must be pointer"
            };
        }

        std::string type_str = arg_type.to_string();

        auto const func_itr = address_of_func_table.find(type_str);
        if (func_itr != std::end(address_of_func_table)) {
            return func_itr->second;
        }

        auto *const prototype = create_func_prototype(
                "__builtin_address_of",
                c.builder.getInt64Ty(),
                {arg_ty}
            );

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

    llvm::Function *emit_fatal_func()
    {
        return create_cached_func_prototype(
                fatal_funcs[0],
                "__dachs_fatal__",
                type_emitter.emit(type::get_unit_type()),
                {}
            );
    }

    llvm::Function *emit_fatal_func(type::type const& arg_type)
    {
        auto *const f = create_cached_func_prototype(
                fatal_funcs[1],
                "__dachs_fatal_reason__",
                type_emitter.emit(type::get_unit_type()),
                {type_emitter.emit(arg_type)}
            );

        f->arg_begin()->setName("reason");
        return f;
    }

    llvm::Function *emit_realloc_func(type::type const& from_ptr_type, type::type const& new_size_type)
    {
        assert(new_size_type.is_builtin("uint"));
        auto const ptr_type = type::get<type::pointer_type>(from_ptr_type);
        assert(ptr_type);

        auto *const ptr_ty = type_emitter.emit(*ptr_type);
        std::string type_str = (*ptr_type)->pointee_type.to_string();

        {
            auto const itr = realloc_func_table.find(type_str);
            if (itr != std::end(realloc_func_table)) {
                return itr->second;
            }
        }

        auto *const size_ty = type_emitter.emit(new_size_type);

        auto *const prototype = create_func_prototype(
                "dachs.realloc." + type_str,
                ptr_ty,
                {ptr_ty, size_ty}
            );

        prototype->addFnAttr(llvm::Attribute::InlineHint);

        auto const ptr_value = prototype->arg_begin();
        ptr_value->setName("ptr");
        auto const size_value = std::next(ptr_value);
        size_value->setName("new_size");

        auto const saved_insert_point = c.builder.GetInsertBlock();
        auto const body = llvm::BasicBlock::Create(c.llvm_context, "entry", prototype);
        c.builder.SetInsertPoint(body);
        c.builder.CreateRet(
                gc_emitter.emit_realloc(
                    ptr_value,
                    c.builder.CreateTrunc(size_value, c.builder.getIntPtrTy(c.data_layout))
                )
            );
        c.builder.SetInsertPoint(saved_insert_point);

        realloc_func_table.emplace(std::move(type_str), prototype);

        return prototype;
    }

    llvm::Function *emit_free_func(type::type const& arg_type)
    {
        auto const ptr_type = type::get<type::pointer_type>(arg_type);
        assert(ptr_type);
        std::string type_str = (*ptr_type)->pointee_type.to_string();

        auto const itr = free_func_table.find(type_str);
        if (itr != std::end(free_func_table)) {
            return itr->second;
        }

        auto *const prototype = create_func_prototype(
                "__builtin_free",
                llvm::StructType::get(c.llvm_context, {})->getPointerTo(),
                {type_emitter.emit(*ptr_type)}
            );

        prototype->addFnAttr(llvm::Attribute::InlineHint);

        auto const arg_value = prototype->arg_begin();
        arg_value->setName("ptr");

        auto *const body = llvm::BasicBlock::Create(c.llvm_context, "entry", prototype);
        auto *const saved_insert_point = c.builder.GetInsertBlock();
        c.builder.SetInsertPoint(body);
        gc_emitter.emit_free(arg_value);

        c.builder.CreateRet(inst_emitter.emit_unit_constant());

        c.builder.SetInsertPoint(saved_insert_point);

        free_func_table.emplace(std::move(type_str), prototype);

        return prototype;
    }

    llvm::Function *emit_gc_operation(llvm::Function *func, std::string const& name)
    {
        if (func) {
            return func;
        }

        auto *const inner_prototype = create_func_prototype(
                "GC_" + name,
                c.builder.getVoidTy(),
                {}
            );
        inner_prototype->addFnAttr(llvm::Attribute::NoUnwind);

        func = create_func_prototype(
                "__builtin_" + name + "_gc",
                llvm::StructType::get(c.llvm_context, {})->getPointerTo(),
                {}
            );

        func->addFnAttr(llvm::Attribute::InlineHint);
        func->addFnAttr(llvm::Attribute::NoUnwind);

        auto *const body = llvm::BasicBlock::Create(c.llvm_context, "entry", func);
        auto *const saved_insert_point = c.builder.GetInsertBlock();

        c.builder.SetInsertPoint(body);
        c.builder.CreateCall(inner_prototype);
        c.builder.CreateRet(inst_emitter.emit_unit_constant());

        c.builder.SetInsertPoint(saved_insert_point);
        return func;
    }

    llvm::Function *emit_enable_gc_func()
    {
        return emit_gc_operation(enable_gc_func, "enable");
    }

    llvm::Function *emit_disable_gc_func()
    {
        return emit_gc_operation(disable_gc_func, "disable");
    }

    // XXX:
    // int GC_disabled(void); returns int, not bool
    llvm::Function *emit_gc_disabled_func()
    {
        return create_cached_func_prototype(
                gc_disabled_func,
                "GC_is_disabled",
                c.builder.getInt1Ty(),
                {}
            );
    }

    llvm::Function *emit(std::string const& name, std::vector<type::type> const& arg_types)
    {
        if (name == "print" || name == "println") {
            assert(arg_types.size() == 1);
            if (auto const b = type::get<type::builtin_type>(arg_types[0])) {
                return emit_print_func(name, *b);
            } else if (auto const p = type::get<type::pointer_type>(arg_types[0])) {
                return emit_print_func(name, *p);
            }
        } else if (name == "__builtin_read_cycle_counter") {
            return emit_read_cycle_counter_func();
        } else if (name == "__builtin_address_of") {
            return emit_address_of_func(arg_types[0]);
        } else if (name == "__builtin_getchar") {
            assert(arg_types.empty());
            return emit_getchar_func();
        } else if (name == "fatal") {
            if (arg_types.empty()) {
                return emit_fatal_func();
            } else {
                return emit_fatal_func(arg_types[0]);
            }
        } else if (name == "__builtin_null?") {
            return emit_is_null_func(arg_types[0]);
        } else if (name == "__builtin_realloc") {
            return emit_realloc_func(arg_types[0], arg_types[1]);
        } else if (name == "__builtin_free") {
            return emit_free_func(arg_types[0]);
        } else if (name == "__builtin_gen_symbol") {
            return emit_gen_symbol_func();
        } else if (name == "__builtin_enable_gc") {
            return emit_enable_gc_func();
        } else if (name == "__builtin_disable_gc") {
            return emit_disable_gc_func();
        } else if (name == "__builtin_gc_disabled?") {
            return emit_gc_disabled_func();
        } // else ...

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_BUILTIN_FUNC_IR_EMITTER_HPP_INCLUDED

