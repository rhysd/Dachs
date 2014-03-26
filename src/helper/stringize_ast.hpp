#if !defined DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED

#include <string>

#include "ast.hpp"

namespace dachs {
namespace helper {

std::string stringize_ast(syntax::ast::ast const& ast);

} // namespace helper
} // namespace dachs



#endif    // DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
