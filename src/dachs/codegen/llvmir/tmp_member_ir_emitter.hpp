#if !defined DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED

#include <string>
#include <cstdint>

#include <boost/variant/static_visitor.hpp>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include "dachs/semantics/type.hpp"
#include "dachs/codegen/llvmir/context.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

class tmp_member_ir_emitter {
    context &ctx;
    using val = llvm::Value *;

    struct type_visit_emitter : boost::static_visitor<val> {
        std::string const& name;
        val const value;
        context &ctx;

        type_visit_emitter(std::string const& n, val const v, context &c) noexcept
            : name(n), value(v), ctx(c)
        {}

        val emit_tuple_access(std::uint64_t const i, type::type const& elem_type)
        {
            assert(value->getType()->isPointerTy() && value->getType()->getPointerElementType()->isStructTy());

            auto access = ctx.builder.CreateStructGEP(value, i);

            if (!elem_type.is_builtin()) {
                access = ctx.builder.CreateLoad(access);
            }

            return access;
        }

        val operator()(type::tuple_type const& t)
        {
            if (name == "size") {
                return ctx.builder.getInt64(t->element_types.size());
            } else if (name == "first") {
                return emit_tuple_access(0u, t->element_types[0u]);
            } else if (name == "second") {
                return emit_tuple_access(1u, t->element_types[1u]);
            } else if (name == "last") {
                return emit_tuple_access(t->element_types.size() - 1, t->element_types[t->element_types.size() - 1]);
            } else {
                return nullptr;
            }
        }

        val operator()(type::array_type const& t)
        {
            if (name == "size") {
                if (t->size) {
                    return ctx.builder.getInt64(*t->size);
                } else {
                    return nullptr;
                }
            }

            return nullptr;
        }

        template<class T>
        val operator()(T const&)
        {
            return nullptr;
        }
    };

public:

    explicit tmp_member_ir_emitter(context &c) noexcept
        : ctx(c)
    {}

    template<class Type>
    val emit_builtin_instance_var(val const child_value, std::string const& member_name, Type &&child_type)
    {
        if (member_name == "__type") {
            return ctx.builder.CreateGlobalStringPtr(child_type.to_string().c_str());
        }

        type_visit_emitter emitter{member_name, child_value, ctx};
        return std::forward<Type>(child_type).apply_visitor(emitter);
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace namespace

#endif    // DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED
