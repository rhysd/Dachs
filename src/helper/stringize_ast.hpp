#if !defined DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED

#include <cstddef>
#include <string>
#include <boost/lexical_cast.hpp>

#include "ast.hpp"
#include "helper/variant.hpp"

namespace dachs {
namespace helper {
namespace detail {

using std::size_t;

class ast_stringizer {
    std::string indent(size_t const level) const
    {
        return std::string(level, ' ');
    }

public:

    std::string visit(syntax::ast::node::program const& p, size_t const indent_level) const
    {
        return indent(indent_level) + p->to_string() + '\n' + visit(p->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::integer_literal const& il, size_t const indent_level) const
    {
        return indent(indent_level) + il->to_string();
    }

    std::string visit(syntax::ast::node::array_literal const& al, size_t const indent_level) const
    {
        return indent(indent_level) + al->to_string();
    }

    std::string visit(syntax::ast::node::tuple_literal const& tl, size_t const indent_level) const
    {
        return indent(indent_level) + tl->to_string();
    }

    std::string visit(syntax::ast::node::literal const& l, size_t const indent_level) const
    {
        std::string const prefix = indent(indent_level) + l->to_string() + '\n';

        if (auto const c = helper::variant::get<char>(l->value)) {
            return prefix + '\'' + *c + '\'';
        } else if (auto const s = helper::variant::get<std::string>(l->value)) {
            return prefix + '"' + *s + '"';
        } else if (auto const int_lit = helper::variant::get<syntax::ast::node::integer_literal>(l->value)) {
            return prefix + visit(*int_lit, indent_level+1);
        } else if (auto const arr_lit = helper::variant::get<syntax::ast::node::array_literal>(l->value)) {
            return prefix + visit(*arr_lit, indent_level+1);
        } else if (auto const tpl_lit = helper::variant::get<syntax::ast::node::tuple_literal>(l->value)) {
            return prefix + visit(*tpl_lit, indent_level+1);
        } else if (auto const b = helper::variant::get<bool>(l->value)) {
            return prefix + indent(indent_level + 1) + (*b ? "true" : "false");
        } else {
            return prefix + indent(indent_level + 1) + boost::lexical_cast<std::string>(l->value);
        }
    }

    std::string visit(syntax::ast::node::identifier const& ident, size_t const indent_level) const
    {
        return indent(indent_level) + ident->to_string();
    }

    std::string visit(syntax::ast::node::primary_expr const& pe, size_t const indent_level) const
    {
        std::string const prefix = indent(indent_level) + pe->to_string() + '\n';
        if (auto const ident = helper::variant::get<syntax::ast::node::identifier>(pe->value)) {
            return prefix + visit(*ident, indent_level+1);
        } else if (auto const lit = helper::variant::get<syntax::ast::node::literal>(pe->value)) {
            return prefix + visit(*lit, indent_level+1);
        } else {
            return prefix + "ERROR";
        }
    }
};

} // namespace detail

inline std::string stringize_ast(syntax::ast::ast const& ast)
{
    return detail::ast_stringizer{}.visit(ast.root, 0);
}

} // namespace helper
} // namespace dachs



#endif    // DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
