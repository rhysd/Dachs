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

    template<class T, class = typename std::enable_if<!is_shared_ptr<T>::value>::type>
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

template<class V> // Workaround for forward declaration
struct node_variant_visitor : public boost::static_visitor<std::string> {
    V const& visitor;
    size_t const indent_level;

    node_variant_visitor(V const& v, size_t const il) : visitor(v), indent_level(il)
    {}

    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const
    {
        return visitor.visit(p, indent_level);
    }
};

class ast_stringizer {
    std::string indent(size_t const level) const
    {
        return std::string(level, '.');
    }

    template<class T>
    std::string prefix_of(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return indent(indent_level) + p->to_string();
    }

    template<class... Args>
    std::string visit_variant_node(boost::variant<Args...> const& v, size_t const indent_level) const
    {
        return boost::apply_visitor(node_variant_visitor<ast_stringizer>{*this, indent_level}, v);
    }

    template<class NodePtrs, class Pred>
    std::string visit_nodes_with_predicate(NodePtrs const& ptrs, Pred const& predicate) const
    {
        return boost::algorithm::join(ptrs | transformed(predicate), "\n");
    }

    template<class NodePtrs>
    std::string visit_nodes(NodePtrs const& ptrs, size_t const indent_level) const
    {
        return visit_nodes_with_predicate(
                    ptrs,
                    [this, indent_level](auto const& p){
                        return visit(p, indent_level);
                    });
    }

    template<class NodePtrs>
    std::string visit_node_variants(NodePtrs const& ptrs, size_t const indent_level) const
    {
        return visit_nodes_with_predicate(
                    ptrs,
                    [this, indent_level](auto const& p){
                        return visit_variant_node(p, indent_level);
                    });
    }

    template<class BinaryOperatorNodePtr>
    std::string visit_binary_operator_ptr(BinaryOperatorNodePtr const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level) + '\n'
                + visit(p->lhs, indent_level+1) + (p->rhss.empty() ? "" : "\n")
                + visit_nodes_with_predicate(
                      p->rhss,
                      [this, indent_level](auto const& op_and_rhs) {
                          return indent(indent_level+1) + "OPERATOR: " + syntax::ast::to_string(op_and_rhs.first)
                              + '\n' + visit(op_and_rhs.second, indent_level+1);
                      });
    }

public:

    std::string visit(syntax::ast::node::program const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level) + '\n' + visit(p->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::literal const& l, size_t const indent_level) const
    {
        return prefix_of(l, indent_level) + '\n' + visit_variant_node(l->value, indent_level+1);
    }

    std::string visit(syntax::ast::node::primary_expr const& pe, size_t const indent_level) const
    {
        return prefix_of(pe, indent_level) + '\n' + visit_variant_node(pe->value, indent_level+1);
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
            + visit_node_variants(pe->postfixes, indent_level+1) + (pe->postfixes.empty() ? "" : "\n")
            + visit(pe->prefix, indent_level+1);
    }

    std::string visit(syntax::ast::node::unary_expr const& ue, size_t const indent_level) const
    {
        return prefix_of(ue, indent_level) + '\n'
                + visit(ue->expr, indent_level+1);
    }

    // TODO: Temporary
    std::string visit(syntax::ast::node::type_name const& tn, size_t const indent_level) const
    {
        return prefix_of(tn, indent_level) + '\n'
                + visit(tn->name, indent_level+1);
    }

    std::string visit(syntax::ast::node::cast_expr const& ce, size_t const indent_level) const
    {
        return prefix_of(ce, indent_level) + '\n'
                + visit_nodes(ce->dest_types, indent_level+1) + (ce->dest_types.empty() ? "" : "\n")
                + visit(ce->source_expr, indent_level+1);
    }

    std::string visit(syntax::ast::node::mult_expr const& me, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(me, indent_level);
    }

    std::string visit(syntax::ast::node::additive_expr const& ae, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(ae, indent_level);
    }

    std::string visit(syntax::ast::node::shift_expr const& se, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(se, indent_level);
    }

    std::string visit(syntax::ast::node::relational_expr const& re, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(re, indent_level);
    }

    // For terminal nodes
    template<class T, class = typename std::enable_if<syntax::ast::is_node<T>::value>::type>
    std::string visit(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level);
    }
};

} // namespace detail

std::string stringize_ast(syntax::ast::ast const& ast)
{
    return detail::ast_stringizer{}.visit(ast.root, 0);
}

} // namespace helper
} // namespace dachs
