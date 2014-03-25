#if !defined DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED
#define      DACHS_HELPER_STRINGIZE_AST_HPP_INCLUDED

#include <cstddef>
#include <string>
#include <type_traits>
#include <boost/lexical_cast.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "ast.hpp"
#include "helper/variant.hpp"

namespace dachs {
namespace helper {
namespace detail {

using std::size_t;

struct to_string : public boost::static_visitor<std::string> {
    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const
    {
        return p->to_string();
    }

    template<class T>
    class is_shared_ptr : std::false_type
    {};

    template<class T>
    class is_shared_ptr<std::shared_ptr<T>> : std::true_type
    {};

    template<class T, class = typename std::enable_if<is_shared_ptr<T>::value>::type>
    std::string operator()(T const& value) const
    {
        return boost::lexical_cast<std::string>(value);
    }
};

struct node_variant_visitor : public boost::static_visitor<std::string> {
    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const
    {
        return p->to_string();
    }
};

class ast_stringizer {
    std::string indent(size_t const level) const
    {
        return std::string(level, ' ');
    }

public:

    // For terminal nodes
    template<class T>
    std::string visit(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return indent(indent_level) + p->to_string();
    }

    std::string visit(syntax::ast::node::program const& p, size_t const indent_level) const
    {
        return indent(indent_level) + p->to_string() + '\n' + visit(p->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::literal const& l, size_t const indent_level) const
    {
        return indent(indent_level) + l->to_string() + '\n' + indent(indent_level+1) + boost::apply_visitor(node_variant_visitor{}, l->value);
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

    std::string visit(syntax::ast::node::index_access const& ia, size_t const indent_level) const
    {
        // TODO: Temporary
        return indent(indent_level) + ia->to_string() + '\n' + visit(ia->prefix_expr, indent_level+1);
    }

    std::string visit(syntax::ast::node::member_access const& ma, size_t const indent_level) const
    {
        return indent(indent_level) + ma->to_string() + '\n'
                + visit(ma->member_name, indent_level+1) + '\n'
                + visit(ma->prefix_expr, indent_level+1);
    }

    std::string visit(syntax::ast::node::argument_expr_list const& ael, size_t const indent_level) const
    {
        // TODO: Temporary
        return indent(indent_level) + ael->to_string();
    }

    std::string visit(syntax::ast::node::function_call const& fc, size_t const indent_level) const
    {
        return indent(indent_level) + fc->to_string() + '\n'
                + visit(fc->args, indent_level+1) + '\n'
                + visit(fc->prefix_expr, indent_level+1);
    }

    std::string visit(syntax::ast::node::postfix_expr const& pe, size_t const indent_level) const
    {
        return indent(indent_level) + pe->to_string() + '\n'
            + indent(indent_level+1) + boost::apply_visitor(node_variant_visitor{}, pe->value);
    }
};

struct literal_stringizer : boost::static_visitor<std::string> {

    ast_stringizer const& stringizer;
    size_t const indent_level;

    literal_stringizer(ast_stringizer const& s, size_t const il)
        : stringizer(s), indent_level(il)
    {}

    std::string operator()(char const c) const
    {
        return {'\'', c, '\''};
    }

    std::string operator()(std::string const& s) const
    {
        return '"' + s + '"';
    }

    std::string operator()(bool const b) const
    {
        return b ? "true" : "false";
    }

    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const
    {
        return stringizer.visit(p, indent_level+1);
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
