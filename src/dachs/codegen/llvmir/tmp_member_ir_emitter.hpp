#if !defined DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED

#include <string>

#include <boost/variant/static_visitor.hpp>
#include <llvm/IR/Value.h>

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

        val operator()(type::tuple_type const& t)
        {
            auto *const ty = value->getType();
            assert(ty->isPointerTy());
            assert(ty->getPointerElementType()->isStructTy());
            if (name == "size") {
                return ctx.builder.getInt64(t->element_types.size());
            } else if (name == "first") {
                return ctx.builder.CreateStructGEP(value, 0u);
            } else if (name == "second") {
                return ctx.builder.CreateStructGEP(value, 1u);
            } else {
                return nullptr;
            }
        }

        val operator()(type::array_type const&)
        {
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
        return child_type.apply_visitor(type_visit_emitter{member_name, child_value, ctx});
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace namespace

#endif    // DACHS_CODEGEN_LLVMIR_TMP_MEMBER_IR_EMITTER_HPP_INCLUDED
