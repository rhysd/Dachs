#if !defined DACHS_SEMANTICS_ERROR_HPP_INCLUDED
#define      DACHS_SEMANTICS_ERROR_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <iostream>

#include <boost/format.hpp>

#include "dachs/semantics/type.hpp"
#include "dachs/ast/ast_fwd.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {
namespace semantics {

template<class Message>
inline void output_semantic_error(std::size_t const line, std::size_t const col, Message const& msg, std::ostream &ost = std::cerr)
{
    helper::colorizer c;
    ost << c.red("Error") << " in line:" << line << ", col:" << col << '\n' << c.bold(msg) << "\n\n";
}

template<class Message>
inline void output_semantic_error(ast::location_type const& location, Message const& msg, std::ostream &ost = std::cerr)
{
    helper::colorizer c;
    ost << c.red("Error") << " in " << location.to_string() << '\n' << c.bold(msg) << "\n\n";
}

template<class Node, class Message>
inline void output_semantic_error(std::shared_ptr<Node> const& node, Message const& msg, std::ostream &ost = std::cerr)
{
    static_assert(ast::traits::is_node<Node>::value, "output_semantic_error(): Node is not AST node.");
    output_semantic_error(node->location, msg, ost);
}

template<class Node1, class Node2>
void print_duplication_error(Node1 const& node1, Node2 const& node2, std::string const& name) noexcept
{
    output_semantic_error(node1, boost::format("  Symbol '%1%' is redefined.\n  Previous definition is at %2%") % name % node2->location.to_string());
}

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_ERROR_HPP_INCLUDED
