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
#include "dachs/codegen/llvmir/allocation_emitter.hpp"
#include "dachs/codegen/llvmir/ir_builder_helper.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/llvm.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {
namespace detail {

using helper::dump;

template<class Emitter>
class tmp_constructor_ir_emitter {
    using val = llvm::Value *;

    context &ctx;
    type_ir_emitter &type_emitter;
    allocation_emitter &alloc_emitter;
    builder::allocation_helper &alloc_helper;
    llvm::Module * const& module;
    Emitter &emitter;

    template<class Values, class Node>
    struct type_ctor_emitter : boost::static_visitor<val> {
        context &ctx;
        type_ir_emitter &type_emitter;
        allocation_emitter &alloc_emitter;
        builder::allocation_helper &alloc_helper;
        llvm::Module &module;
        Values const& arg_values;
        std::shared_ptr<Node> const& node;
        Emitter &emitter;

        type_ctor_emitter(
                context &c,
                type_ir_emitter &t,
                allocation_emitter &ae,
                builder::allocation_helper &a,
                llvm::Module &m,
                Values const& vs,
                std::shared_ptr<Node> const& n,
                Emitter &e
        ) noexcept
            : ctx(c)
            , type_emitter(t)
            , alloc_emitter(ae)
            , alloc_helper(a)
            , module(m)
            , arg_values(vs)
            , node(n)
            , emitter(e)
        {
            assert(vs.size() <= 2);
        }

        bool should_deref(llvm::Value *const v, type::type const& t)
        {
            if (t.is_builtin()) {
                return false;
            }

            return v->getType()->getPointerElementType()->isPointerTy();
        }

        val operator()(type::pointer_type const& p)
        {
            assert(arg_values.size() == 1u);
            return alloc_emitter.emit_malloc(p->pointee_type, arg_values[0]);
        }

        val operator()(type::array_type const& a)
        {
            if (arg_values.empty()) {
                auto *const elem_ty = llvm::dyn_cast<llvm::PointerType>(type_emitter.emit(a));
                assert(elem_ty->isPointerTy());
                return llvm::ConstantPointerNull::get(elem_ty);
            }

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

                auto const constant = new llvm::GlobalVariable(
                            module,
                            ty,
                            true /*constant*/,
                            llvm::GlobalValue::PrivateLinkage,
                            llvm::ConstantArray::get(ty, elems)
                        );

                constant->setUnnamedAddr(true);
                return ctx.builder.CreateConstInBoundsGEP2_32(constant, 0u, 0u);

            } else {
                auto *const allocated = alloc_helper.create_alloca(a);

                if (arg_values.size() != 2) {
                    return allocated;
                }

                for (auto const idx : helper::indices(size)) {
                    auto *const dest = ctx.builder.CreateConstInBoundsGEP1_32(allocated, idx);

                    if (auto const copier = emitter.semantics_ctx.copier_of(a->element_type)) {
                        val const copied
                            = emitter.emit_copier_call(
                                    node,
                                    arg_values[1],
                                    *copier
                                );
                        ctx.builder.CreateStore(copied, dest);
                    } else {
                        alloc_helper.create_deep_copy(
                                arg_values[1],
                                should_deref(dest, a->element_type)
                                    ? ctx.builder.CreateLoad(dest)
                                    : dest,
                                a->element_type
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

    tmp_constructor_ir_emitter(
            context &c,
            type_ir_emitter &t,
            allocation_emitter &ae,
            builder::allocation_helper &a,
            llvm::Module *const& m,
            Emitter &e
    ) noexcept
        : ctx(c)
        , type_emitter(t)
        , alloc_emitter(ae)
        , alloc_helper(a)
        , module(m)
        , emitter(e)
    {}

    template<class Values, class Node>
    val emit(type::type &type, Values const& arg_values, std::shared_ptr<Node> const& node)
    {
        assert(module);
        type_ctor_emitter<Values, Node> ctor_emitter{
                ctx,
                type_emitter,
                alloc_emitter,
                alloc_helper,
                *module, 
                arg_values,
                node,
                emitter
            };
        return type.apply_visitor(ctor_emitter);
    }
};

} // namespace detail
} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_TMP_CONSTRUCTOR_IR_EMITTER_HPP_INCLUDED
