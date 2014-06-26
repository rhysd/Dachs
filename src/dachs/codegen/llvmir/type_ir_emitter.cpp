#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DerivedTypes.h>

#include "dachs/semantics/type.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

namespace detail {

using helper::variant::apply_lambda;

class type_ir_emitter {
    llvm::LLVMContext &context;

    template<class String>
    void error(String const& msg)
    {
        throw code_generation_error{"LLVM IR generator", msg};
    }

public:

    explicit type_ir_emitter(llvm::LLVMContext &c)
        : context(c)
    {}

    llvm::Type *emit(type::builtin_type const& builtin)
    {
        llvm::Type *result;

        if (builtin->name == "int") {
            result = llvm::Type::getInt64Ty(context);
        } else if (builtin->name == "uint") {
            result = llvm::Type::getInt64Ty(context);
        } else if (builtin->name == "float") {
            result = llvm::Type::getDoubleTy(context);
        } else if (builtin->name == "char") {
            result = llvm::Type::getInt8Ty(context);
        } else if (builtin->name == "bool") {
            result = llvm::Type::getInt1Ty(context);
        } else if (builtin->name == "string") {
            throw not_implemented_error{__FILE__, __func__, __LINE__, "string type LLVM IR generation"};
        } else if (builtin->name == "symbol") {
            throw not_implemented_error{__FILE__, __func__, __LINE__, "symbol type LLVM IR generation"};
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        if (!result) {
            error("Failed to emit a builtin type: " + builtin->to_string());
        }

        return result;
    }

    llvm::Type *emit(type::class_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "class type LLVM IR generation"};
    }

    llvm::Type *emit(type::tuple_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "tuple type LLVM IR generation"};
    }

    llvm::Type *emit(type::func_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "function type LLVM IR generation"};
    }

    llvm::Type *emit(type::proc_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "procedure type LLVM IR generation"};
    }

    llvm::Type *emit(type::func_ref_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "function reference type LLVM IR generation"};
    }

    llvm::Type *emit(type::dict_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "dictionary type LLVM IR generation"};
    }

    llvm::Type *emit(type::array_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "array type LLVM IR generation"};
    }

    llvm::Type *emit(type::range_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "range type LLVM IR generation"};
    }

    llvm::Type *emit(type::qualified_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "qualified type LLVM IR generation"};
    }

    llvm::Type *emit(type::template_type const&)
    {
        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }
};

} // namespace detail

llvm::Type *emit_type_ir(type::type const& t, llvm::LLVMContext &context)
{
    return helper::variant::apply_lambda(
                [&context](auto const& t)
                {
                    return detail::type_ir_emitter{context}.emit(t);
                }, t.raw_value()
            );
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs
