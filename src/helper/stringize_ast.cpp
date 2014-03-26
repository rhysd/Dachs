#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <type_traits>
#include <cstddef>
#include <boost/lexical_cast.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/join.hpp>

#include "stringize_ast.hpp"
#include "helper/variant.hpp"

namespace dachs {
namespace helper {
namespace detail {

using std::size_t;
using boost::algorithm::join;
using boost::adaptors::transformed;

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
        try {
            return boost::lexical_cast<std::string>(value);
        }
        catch(boost::bad_lexical_cast const& e) {
            return e.what();
        }
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

    template<class T>
    std::string prefix_of(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return indent(indent_level) + p->to_string();
    }

    template<class... Args>
    std::string variant_prefix_of(boost::variant<Args...> const& v, size_t const indent_level) const
    {
        return indent(indent_level) + boost::apply_visitor(node_variant_visitor{}, v);
    }

public:

    // For terminal nodes
    template<class T>
    std::string visit(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level);
    }

    std::string visit(syntax::ast::node::program const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level) + '\n' + visit(p->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::literal const& l, size_t const indent_level) const
    {
        return prefix_of(l, indent_level) + '\n' + variant_prefix_of(l->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::primary_expr const& pe, size_t const indent_level) const
    {
        std::string const prefix = prefix_of(pe, indent_level) + '\n';
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
        return prefix_of(ia, indent_level);
    }

    std::string visit(syntax::ast::node::member_access const& ma, size_t const indent_level) const
    {
        return prefix_of(ma, indent_level) + '\n'
                + visit(ma->member_name, indent_level+1);
    }

    std::string visit(syntax::ast::node::function_call const& fc, size_t const indent_level) const
    {
        // TODO: Temporary
        return prefix_of(fc, indent_level);
    }

    std::string visit(syntax::ast::node::postfix_expr const& pe, size_t const indent_level) const
    {
        return prefix_of(pe, indent_level) + '\n'
            + join(
                pe->postfixes | transformed([this, indent_level](auto const& postfix){
                    return variant_prefix_of(postfix, indent_level+1);
                })
                , "\n"
            ) + '\n'
            + visit(pe->prefix, indent_level+1);
    }
};

} // namespace detail

std::string stringize_ast(syntax::ast::ast const& ast)
{
    return detail::ast_stringizer{}.visit(ast.root, 0);
}

} // namespace helper
} // namespace dachs
