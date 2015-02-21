#if !defined DACHS_CODEGEN_LLVMIR_TMP_CONSTRUCTOR_IR_EMITTER_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_TMP_CONSTRUCTOR_IR_EMITTER_HPP_INCLUDED

#include <string>
#include <cstdint>

#include <boost/variant/static_visitor.hpp>
#include <llvm/IR/Value.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/codegen/llvmir/type_ir_emitter.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

class tmp_constructor_ir_emitter {
    using val = llvm::Value *;

    context &ctx;
    type_ir_emitter &type_emitter;

    template<class Values>
    struct type_ctor_emitter : boost::static_visitor<val> {
        context &ctx;
        type_ir_emitter &type_emitter;
        Values const& arg_values;

        type_ctor_emitter(context &c, type_ir_emitter &t, Values const& a)
            : ctx(c), type_emitter(t), arg_values(a)
        {
            assert(a.size() == 1 || a.size() == 2);
        }

        val operator()(type::array_type const& a)
        {
            if (!llvm::isa<llvm::ConstantInt>(arg_values[0])) {
                return nullptr;
            }

            assert(a->size);
            auto *const ty = type_emitter.emit_alloc_fixed_array(a);
            auto const size = *a->size;

            // Note:
            // 0-cleared array will be modified.  It should be allocated by alloca.

            if (arg_values.size() == 2 && llvm::isa<llvm::Constant>(arg_values[1])) {
                auto *const elem_constant = llvm::dyn_cast<llvm::Constant>(arg_values[1]);
                std::vector<llvm::Constant *> elems;
                elems.reserve(size);
                for (auto const unused : helper::indices(size)) {
                    (void) unused;
                    elems.push_back(elem_constant);
                }
                return llvm::ConstantArray::get(ty, elems);

            } else {
                auto *const allocated = ctx.builder.CreateAlloca(ty);

                if (arg_values.size() == 1) {
                    ctx.builder.CreateMemSet(
                            allocated,
                            ctx.builder.getInt8(0u),
                            ctx.data_layout->getTypeAllocSize(ty),
                            ctx.data_layout->getPrefTypeAlignment(ty)
                        );
                } else {
                    for (auto const idx : helper::indices(size)) {
                        ctx.builder.CreateStore(
                                arg_values[1],
                                ctx.builder.CreateStructGEP(allocated, idx)
                            );
                    }
                }

                return allocated;
            }
        }

        template<class T>
        val operator()(T const&)
        {
            return nullptr;
        }
    };

public:

    tmp_constructor_ir_emitter(context &c, type_ir_emitter &t) noexcept
        : ctx(c), type_emitter(t)
    {}

    template<class Values>
    val emit(type::type &type, Values const& arg_values)
    {
        type_ctor_emitter<Values> emitter{ctx, type_emitter, arg_values};
        return type.apply_visitor(emitter);
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_TMP_CONSTRUCTOR_IR_EMITTER_HPP_INCLUDED
