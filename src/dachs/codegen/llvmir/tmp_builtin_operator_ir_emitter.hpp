#if !defined CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP
#define      CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP

#include <string>

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>

#include <boost/range/irange.hpp>

#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

using helper::indices;

class tmp_builtin_bin_op_ir_emitter {
    context& ctx;
    llvm::Value *const lhs, *const rhs;
    std::string const& op;

    using val = llvm::Value *;
    using self_type = tmp_builtin_bin_op_ir_emitter;

    template<class String>
    bool is_relational(String const& op) const noexcept
    {
        return helper::any_of({"==", "!=", ">", "<", ">=", "<="}, op);
    }

public:

    tmp_builtin_bin_op_ir_emitter(
                context &c,
                llvm::Value *const l,
                llvm::Value *const r,
                std::string const& op
            ) noexcept
        : ctx(c)
        , lhs(l)
        , rhs(r)
        , op(op)
    {}

    val emit(type::type const& t) noexcept
    {
        if (auto const bt = type::get<type::builtin_type>(t)) {
            return emit(*bt);
        } else if (auto const at = type::get<type::array_type>(t)) {
            return emit(*at);
        } else if (auto const tt = type::get<type::tuple_type>(t)) {
            return emit(*tt);
        } else if (auto const rt = type::get<type::range_type>(t)) {
            return emit(*rt);
        } else {
            return nullptr;
        }
    }

    val emit(type::builtin_type const& builtin) noexcept
    {
        bool const is_float = builtin->name == "float";
        bool const is_int = builtin->name == "int" || builtin->name == "bool" || builtin->name == "char";
        bool const is_uint = builtin->name == "uint";

        if (op == ">>") {
            return ctx.builder.CreateAShr(lhs, rhs, "shrtmp");
        } else if (op == "<<") {
            return ctx.builder.CreateShl(lhs, rhs, "shltmp");
        } else if (op == "*") {
            if (is_int || is_uint) {
                return ctx.builder.CreateMul(lhs, rhs, "multmp");
            } else if (is_float) {
                return ctx.builder.CreateFMul(lhs, rhs, "fmultmp");
            }
        } else if (op == "/") {
            if (is_int) {
                return ctx.builder.CreateSDiv(lhs, rhs, "sdivtmp");
            } else if (is_uint) {
                return ctx.builder.CreateUDiv(lhs, rhs, "udivtmp");
            } else if (is_float) {
                return ctx.builder.CreateFDiv(lhs, rhs, "fdivtmp");
            }
        } else if (op == "%") {
            if (is_int) {
                return ctx.builder.CreateSRem(lhs, rhs, "sremtmp");
            } else if (is_uint) {
                return ctx.builder.CreateURem(lhs, rhs, "uremtmp");
            } else if (is_float) {
                return ctx.builder.CreateFRem(lhs, rhs, "fremtmp");
            }
        } else if (op == "+") {
            if (is_int || is_uint) {
                return ctx.builder.CreateAdd(lhs, rhs, "addtmp");
            } else if (is_float) {
                return ctx.builder.CreateFAdd(lhs, rhs, "faddtmp");
            }
        } else if (op == "-") {
            if (is_int || is_uint) {
                return ctx.builder.CreateSub(lhs, rhs, "subtmp");
            } else if (is_float) {
                return ctx.builder.CreateFSub(lhs, rhs, "fsubtmp");
            }
        } else if (op == "&") {
            return ctx.builder.CreateAnd(lhs, rhs, "andtmp");
        } else if (op == "^") {
            return ctx.builder.CreateXor(lhs, rhs, "xortmp");
        } else if (op == "|") {
            return ctx.builder.CreateOr(lhs, rhs, "ortmp");
        } else if (op == "<") {
            if (is_int) {
                return ctx.builder.CreateICmpSLT(lhs, rhs, "icmpslttmp");
            } else if (is_uint) {
                return ctx.builder.CreateICmpULT(lhs, rhs, "icmpulttmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpULT(lhs, rhs, "fcmpulttmp");
            }
        } else if (op == ">") {
            if (is_int) {
                return ctx.builder.CreateICmpSGT(lhs, rhs, "icmpsgttmp");
            } else if (is_uint) {
                return ctx.builder.CreateICmpUGT(lhs, rhs, "icmpugttmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpUGT(lhs, rhs, "fcmpugttmp");
            }
        } else if (op == "<=") {
            if (is_int) {
                return ctx.builder.CreateICmpSLE(lhs, rhs, "icmpsletmp");
            } else if (is_uint) {
                return ctx.builder.CreateICmpULE(lhs, rhs, "icmpuletmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpULE(lhs, rhs, "fcmpuletmp");
            }
        } else if (op == ">=") {
            if (is_int) {
                return ctx.builder.CreateICmpSGE(lhs, rhs, "icmpsgetmp");
            } else if (is_uint) {
                return ctx.builder.CreateICmpUGE(lhs, rhs, "icmpugetmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpUGE(lhs, rhs, "fcmpugetmp");
            }
        } else if (op == "==") {
            if (is_int || is_uint) {
                return ctx.builder.CreateICmpEQ(lhs, rhs, "icmpeqtmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpUEQ(lhs, rhs, "fcmpeqtmp");
            }
        } else if (op == "!=") {
            if (is_int || is_uint) {
                return ctx.builder.CreateICmpNE(lhs, rhs, "icmpnetmp");
            } else if (is_float) {
                return ctx.builder.CreateFCmpUNE(lhs, rhs, "fcmpnetmp");
            }
        } else if (op == "&&") {
            if (!is_float) {
                return ctx.builder.CreateAnd(lhs, rhs, "andltmp");
            }
        } else if (op == "||") {
            if (!is_float) {
                return ctx.builder.CreateOr(lhs, rhs, "orltmp");
            }
        }

        return nullptr;
    }

    val emit(type::array_type const&) noexcept
    {
        // TODO
        return nullptr;
    }

    val emit(type::tuple_type const& tuple) noexcept
    {
        assert(lhs->getType()->isPointerTy());
        assert(rhs->getType()->isPointerTy());
        auto const& elem_types = tuple->element_types;

        if (!is_relational(op)) {
            return nullptr;
        }

        auto const emit_elem_compare
            = [&](auto const idx, auto const& o)
            {
                auto *const lhs_elem_val = ctx.builder.CreateLoad(ctx.builder.CreateStructGEP(lhs, idx));
                auto *const rhs_elem_val = ctx.builder.CreateLoad(ctx.builder.CreateStructGEP(rhs, idx));
                return self_type{ctx, lhs_elem_val, rhs_elem_val, o}.emit(elem_types[idx]);
            };

        if (op == "==" || op == "!=") {
            val folding_value = emit_elem_compare(0u, op);
            for (auto const idx : indices(1u, elem_types.size())) {
                auto *const next_elem_val = emit_elem_compare(idx, op);
                if (!folding_value || !next_elem_val) {
                    return nullptr;
                }
                folding_value = self_type{ctx, folding_value, next_elem_val, "&&"}.emit(type::get_builtin_type("bool", type::no_opt));
            }

            return folding_value;
        } else {
            // Note:
            // '<=' is '== or <'
            std::string const ineq_op = {op[0]}; // '<=' to '<'
            val folding_value = emit_elem_compare(0u, ineq_op);

            for (auto const idx : indices(1u, elem_types.size())) {
                auto *const next_elem_val = emit_elem_compare(idx, ineq_op);
                if (!folding_value || !next_elem_val) {
                    return nullptr;
                }
                folding_value = self_type{ctx, folding_value, next_elem_val, "||"}.emit(type::get_builtin_type("bool", type::no_opt));
            }

            bool const includes_equal = op == "<=" || op == ">=";
            if (includes_equal) {
                folding_value =
                    self_type{
                        ctx,
                        self_type{ctx, lhs, rhs, "=="}.emit(tuple),
                        folding_value,
                        "||"
                    }.emit(type::get_builtin_type("bool", type::no_opt));
            }

            return folding_value;
        }
    }

    val emit(type::range_type const&) noexcept
    {
        // TODO
        return nullptr;
    }
};

// TODO
class tmp_builtin_unary_op_ir_emitter{
    context &ctx;
    llvm::Value *const value;
    std::string const& op;

    using val = llvm::Value *;

public:

    tmp_builtin_unary_op_ir_emitter(
                context &c,
                llvm::Value *const v,
                std::string const& op
            )
        : ctx(c)
        , value(v)
        , op(op)
    {}

    val emit(type::builtin_type const& builtin)
    {
        bool const is_float = builtin->name == "float";
        bool const is_int = builtin->name == "int";

        if (op == "+") {
            // Note: Do nothing.
            return value;
        } else if (op == "-") {
            if (is_int) {
                return ctx.builder.CreateNeg(value, "negtmp");
            } else if (is_float) {
                return ctx.builder.CreateFNeg(value, "fnegtmp");
            }
        } else if (op == "~" || op == "!") {
            if (!is_float) {
                return ctx.builder.CreateNot(value, "nottmp");
            }
        }

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP
