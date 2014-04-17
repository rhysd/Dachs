#if !defined DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED

#include <string>

#include "dachs/ast.hpp"

namespace dachs {
namespace helper {

std::string stringize_ast(ast::ast const& ast, bool const colorful);

} // namespace helper
} // namespace dachs



#endif    // DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
