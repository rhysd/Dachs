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

        val emit_tuple_access(std::uint64_t const i)
        {
            auto *const t = value->getType();
            assert(t->isStructTy() || (t->isPointerTy() && t->getPointerElementType()->isStructTy()));

            return t->isStructTy() ?
                       ctx.builder.CreateExtractValue(value, i) :
                       ctx.builder.CreateStructGEP(value, i);
        }

        val operator()(type::tuple_type const& t)
        {
            if (name == "size") {
                return ctx.builder.getInt64(t->element_types.size());
            } else if (name == "first") {
                return emit_tuple_access(0u);
            } else if (name == "second") {
                return emit_tuple_access(1u);
            } else if (name == "last") {
                return emit_tuple_access(t->element_types.size() - 1);
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
                    auto *ty = value->getType();
                    if (ty->isPointerTy()) {
                        ty = ty->getPointerElementType();
                    }

                    if (!llvm::isa<llvm::ArrayType>(ty)) {
                        return nullptr;
                    }

                    return ctx.builder.getInt64(ty->getArrayNumElements());
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

    val emit_var(val const child_value, std::string const& member_name, type::type &&child_type)
    {
        if (member_name == "__type") {
            return ctx.builder.CreateGlobalStringPtr(child_type.to_string().c_str());
        }

        return child_type.apply_visitor(type_visit_emitter{member_name, child_value, ctx});
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace namespace

#endif    // DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED
