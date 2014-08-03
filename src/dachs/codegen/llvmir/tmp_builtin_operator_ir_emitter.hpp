#if !defined CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP
#define      CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP

#include <string>

#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>

#include <boost/range/irange.hpp>

#include "dachs/semantics/type.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

using helper::indices;

class tmp_builtin_bin_op_ir_emitter {
    llvm::IRBuilder<> &builder;
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
                llvm::IRBuilder<> &b,
                llvm::Value *const l,
                llvm::Value *const r,
                std::string const& op
            ) noexcept
        : builder(b)
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
            return builder.CreateAShr(lhs, rhs, "shrtmp");
        } else if (op == "<<") {
            return builder.CreateShl(lhs, rhs, "shltmp");
        } else if (op == "*") {
            if (is_int || is_uint) {
                return builder.CreateMul(lhs, rhs, "multmp");
            } else if (is_float) {
                return builder.CreateFMul(lhs, rhs, "fmultmp");
            }
        } else if (op == "/") {
            if (is_int) {
                return builder.CreateSDiv(lhs, rhs, "sdivtmp");
            } else if (is_uint) {
                return builder.CreateUDiv(lhs, rhs, "udivtmp");
            } else if (is_float) {
                return builder.CreateFDiv(lhs, rhs, "fdivtmp");
            }
        } else if (op == "%") {
            if (is_int) {
                return builder.CreateSRem(lhs, rhs, "sremtmp");
            } else if (is_uint) {
                return builder.CreateURem(lhs, rhs, "uremtmp");
            } else if (is_float) {
                return builder.CreateFRem(lhs, rhs, "fremtmp");
            }
        } else if (op == "+") {
            if (is_int || is_uint) {
                return builder.CreateAdd(lhs, rhs, "addtmp");
            } else if (is_float) {
                return builder.CreateFAdd(lhs, rhs, "faddtmp");
            }
        } else if (op == "-") {
            if (is_int || is_uint) {
                return builder.CreateSub(lhs, rhs, "subtmp");
            } else if (is_float) {
                return builder.CreateFSub(lhs, rhs, "fsubtmp");
            }
        } else if (op == "&") {
            return builder.CreateAnd(lhs, rhs, "andtmp");
        } else if (op == "^") {
            return builder.CreateXor(lhs, rhs, "xortmp");
        } else if (op == "|") {
            return builder.CreateOr(lhs, rhs, "ortmp");
        } else if (op == "<") {
            if (is_int) {
                return builder.CreateICmpSLT(lhs, rhs, "icmpslttmp");
            } else if (is_uint) {
                return builder.CreateICmpULT(lhs, rhs, "icmpulttmp");
            } else if (is_float) {
                return builder.CreateFCmpULT(lhs, rhs, "fcmpulttmp");
            }
        } else if (op == ">") {
            if (is_int) {
                return builder.CreateICmpSGT(lhs, rhs, "icmpsgttmp");
            } else if (is_uint) {
                return builder.CreateICmpUGT(lhs, rhs, "icmpugttmp");
            } else if (is_float) {
                return builder.CreateFCmpUGT(lhs, rhs, "fcmpugttmp");
            }
        } else if (op == "<=") {
            if (is_int) {
                return builder.CreateICmpSLE(lhs, rhs, "icmpsletmp");
            } else if (is_uint) {
                return builder.CreateICmpULE(lhs, rhs, "icmpuletmp");
            } else if (is_float) {
                return builder.CreateFCmpULE(lhs, rhs, "fcmpuletmp");
            }
        } else if (op == ">=") {
            if (is_int) {
                return builder.CreateICmpSGE(lhs, rhs, "icmpsgetmp");
            } else if (is_uint) {
                return builder.CreateICmpUGE(lhs, rhs, "icmpugetmp");
            } else if (is_float) {
                return builder.CreateFCmpUGE(lhs, rhs, "fcmpugetmp");
            }
        } else if (op == "==") {
            if (is_int || is_uint) {
                return builder.CreateICmpEQ(lhs, rhs, "icmpeqtmp");
            } else if (is_float) {
                return builder.CreateFCmpUEQ(lhs, rhs, "fcmpeqtmp");
            }
        } else if (op == "!=") {
            if (is_int || is_uint) {
                return builder.CreateICmpNE(lhs, rhs, "icmpnetmp");
            } else if (is_float) {
                return builder.CreateFCmpUNE(lhs, rhs, "fcmpnetmp");
            }
        } else if (op == "&&") {
            if (!is_float) {
                return builder.CreateAnd(lhs, rhs, "andltmp");
            }
        } else if (op == "||") {
            if (!is_float) {
                return builder.CreateOr(lhs, rhs, "orltmp");
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

        if (op == "==" || op == "!=") {
            for (auto const idx : indices(elem_types.size())) {
                auto *const lhs_elem_val = builder.CreateStructGEP(lhs, idx);
                auto *const rhs_elem_val = builder.CreateStructGEP(rhs, idx);
                auto *const elem_compared_val = self_type{builder, lhs_elem_val, rhs_elem_val, op}.emit(elem_types[idx]);
                if (!elem_compared_val) {
                    return nullptr;
                }

                // TODO
            }
        } else if (op == ">" || op == "<") {
            
        } else {
            
        }
        return nullptr;
    }

    val emit(type::range_type const&) noexcept
    {
        // TODO
        return nullptr;
    }
};

// TODO
class tmp_builtin_unary_op_ir_emitter{
    llvm::IRBuilder<> &builder;
    llvm::Value *const value;
    std::string const& op;

    using val = llvm::Value *;

public:

    tmp_builtin_unary_op_ir_emitter(
                llvm::IRBuilder<> &b,
                llvm::Value *const v,
                std::string const& op
            )
        : builder(b)
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
                return builder.CreateNeg(value, "negtmp");
            } else if (is_float) {
                return builder.CreateFNeg(value, "fnegtmp");
            }
        } else if (op == "~" || op == "!") {
            if (!is_float) {
                return builder.CreateNot(value, "nottmp");
            }
        }

        return nullptr;
    }
};

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // CODEGEN_LLVMIR_TMP_BUILTIN_OPERATOR_IR_EMITTER_HPP
