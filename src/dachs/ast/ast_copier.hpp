#if !defined DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
#define      DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED

namespace dachs {
namespace ast {

template<class ASTNode>
ASTNode copy_ast(ASTNode const& node);

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
