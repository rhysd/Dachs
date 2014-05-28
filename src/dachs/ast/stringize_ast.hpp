#if !defined DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED

#include <string>

#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace ast {

std::string stringize_ast(ast::ast const& ast, bool const colorful);

} // namespace ast
} // namespace dachs



#endif    // DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
