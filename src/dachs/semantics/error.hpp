#if !defined DACHS_SEMANTICS_ERROR_HPP_INCLUDED
#define      DACHS_SEMANTICS_ERROR_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <iostream>

#include "dachs/semantics/type.hpp"
#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace semantics {

template<class Message>
inline void output_semantic_error(std::size_t const line, std::size_t const col, Message const& msg, std::ostream &ost = std::cerr)
{
    ost << "Semantic error at line:" << line << ", col:" << col << '\n' << msg << std::endl;
}

template<class Node, class Message>
inline void output_semantic_error(std::shared_ptr<Node> const& node, Message const& msg, std::ostream &ost = std::cerr)
{
    static_assert(ast::traits::is_node<Node>::value, "output_semantic_error(): Node is not AST node.");
    ost << "Semantic error at line:" << node->line << ", col:" << node->col << '\n' << msg << std::endl;
}

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_ERROR_HPP_INCLUDED
