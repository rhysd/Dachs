#if !defined DACHS_WARNING_HPP_INCLUDED
#define      DACHS_WARNING_HPP_INCLUDED

#include <cstddef>
#include <iostream>
#include <type_traits>
#include <memory>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {

template<class Message>
inline void output_warning(Message const& msg, std::ostream &ost = std::cerr)
{
    helper::colorizer c;
    ost << c.yellow("Warning") << ": " << msg << std::endl;
}

template<class Message>
inline void output_warning(std::size_t const line, std::size_t const col, Message const& msg, std::ostream &ost = std::cerr)
{
    helper::colorizer c;
    ost << c.yellow("Warning") << " at line:" << line << ", col:" << col << '\n' << msg << std::endl;
}

template<class Node, class Message>
inline
auto output_warning(std::shared_ptr<Node> const& node, Message const& msg, std::ostream &ost = std::cerr)
    -> typename std::enable_if<ast::traits::is_node<Node>::value>::type
{
    helper::colorizer c;
    ost << c.yellow("Warning") << " at line:" << node->line << ", col:" << node->col << '\n' << msg << std::endl;
}

} // namespace dachs

#endif    // DACHS_WARNING_HPP_INCLUDED
