#include <llvm/IR/Module.h>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

llvm::Module &generate_llvm_ir(ast::ast const& a, scope::scope_tree const& t);

} // namespace llvm
} // namespace codegen
} // namespace dachs
