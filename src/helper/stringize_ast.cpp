#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <type_traits>
#include <cstddef>
#include <boost/lexical_cast.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/numeric.hpp>

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

    template<class T>
    std::string visit_optional_node(boost::optional<T> const& o, size_t const indent_level) const
    {
        return (o ? '\n' + visit(*o, indent_level) : "");
    }

    template<class NodePtrs, class Pred>
    std::string visit_nodes_with_predicate(NodePtrs const& ptrs, Pred const& predicate) const
    {
        return boost::accumulate(ptrs | transformed(predicate)
                                 , std::string{}
                                 , [](auto const& acc, auto const& item){
                                     return acc + '\n' + item;
                                 });
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
                + visit(p->lhs, indent_level+1)
                + visit_nodes_with_predicate(
                      p->rhss,
                      [this, indent_level](auto const& op_and_rhs) {
                          return indent(indent_level+1) + "OPERATOR: " + ast::to_string(op_and_rhs.first)
                              + '\n' + visit(op_and_rhs.second, indent_level+1);
                      });
    }

public:

    std::string visit(ast::node::program const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level) + visit_nodes(p->value, indent_level+1);
    }

    std::string visit(ast::node::array_literal const& al, size_t const indent_level) const
    {
        return prefix_of(al, indent_level) + visit_nodes(al->element_exprs, indent_level+1);
    }

    std::string visit(ast::node::tuple_literal const& tl, size_t const indent_level) const
    {
        return prefix_of(tl, indent_level) + visit_nodes(tl->element_exprs, indent_level+1);
    }

    std::string visit(ast::node::literal const& l, size_t const indent_level) const
    {
        return prefix_of(l, indent_level) + '\n' + visit_variant_node(l->value, indent_level+1);
    }

    std::string visit(ast::node::primary_expr const& pe, size_t const indent_level) const
    {
        return prefix_of(pe, indent_level) + '\n' + visit_variant_node(pe->value, indent_level+1);
    }

    std::string visit(ast::node::index_access const& ia, size_t const indent_level) const
    {
        return prefix_of(ia, indent_level) + '\n' + visit(ia->index_expr, indent_level+1);
    }

    std::string visit(ast::node::member_access const& ma, size_t const indent_level) const
    {
        return prefix_of(ma, indent_level) + '\n'
                + visit(ma->member_name, indent_level+1);
    }

    std::string visit(ast::node::function_call const& fc, size_t const indent_level) const
    {
        return prefix_of(fc, indent_level) + visit_nodes(fc->args, indent_level+1);
    }

    std::string visit(ast::node::postfix_expr const& pe, size_t const indent_level) const
    {
        return prefix_of(pe, indent_level)
            + visit_node_variants(pe->postfixes, indent_level+1) + '\n'
            + visit(pe->prefix, indent_level+1);
    }

    std::string visit(ast::node::unary_expr const& ue, size_t const indent_level) const
    {
        return prefix_of(ue, indent_level) + '\n'
                + visit(ue->expr, indent_level+1);
    }

    // TODO: Temporary
    std::string visit(ast::node::type_name const& tn, size_t const indent_level) const
    {
        return prefix_of(tn, indent_level) + '\n'
                + visit(tn->name, indent_level+1);
    }

    std::string visit(ast::node::cast_expr const& ce, size_t const indent_level) const
    {
        return prefix_of(ce, indent_level)
                + visit_nodes(ce->dest_types, indent_level+1) + '\n'
                + visit(ce->source_expr, indent_level+1);
    }

    std::string visit(ast::node::mult_expr const& me, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(me, indent_level);
    }

    std::string visit(ast::node::additive_expr const& ae, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(ae, indent_level);
    }

    std::string visit(ast::node::shift_expr const& se, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(se, indent_level);
    }

    std::string visit(ast::node::relational_expr const& re, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(re, indent_level);
    }

    std::string visit(ast::node::equality_expr const& ee, size_t const indent_level) const
    {
        return visit_binary_operator_ptr(ee, indent_level);
    }

    std::string visit(ast::node::and_expr const& ae, size_t const indent_level) const
    {
        return prefix_of(ae, indent_level) + visit_nodes(ae->exprs, indent_level+1);
    }

    std::string visit(ast::node::xor_expr const& xe, size_t const indent_level) const
    {
        return prefix_of(xe, indent_level) + visit_nodes(xe->exprs, indent_level+1);
    }

    std::string visit(ast::node::or_expr const& oe, size_t const indent_level) const
    {
        return prefix_of(oe, indent_level) + visit_nodes(oe->exprs, indent_level+1);
    }

    std::string visit(ast::node::logical_and_expr const& lae, size_t const indent_level) const
    {
        return prefix_of(lae, indent_level) + visit_nodes(lae->exprs, indent_level+1);
    }

    std::string visit(ast::node::logical_or_expr const& loe, size_t const indent_level) const
    {
        return prefix_of(loe, indent_level) + visit_nodes(loe->exprs, indent_level+1);
    }

    std::string visit(ast::node::expression const& e, size_t const indent_level) const
    {
        return prefix_of(e, indent_level) + '\n'
               + visit(e->child_expr, indent_level+1)
               + visit_optional_node(e->maybe_type, indent_level+1);
    }

    std::string visit(ast::node::assignment_stmt const& as, size_t const indent_level) const
    {
        return prefix_of(as, indent_level)
               + visit_nodes(as->assignees, indent_level+1)
               + '\n' + indent(indent_level+1) + "ASSIGN_OPERATOR: " + ast::to_string(as->assign_op)
               + visit_nodes(as->rhs_exprs, indent_level+1);
    }

    std::string visit(ast::node::if_stmt const& is, size_t const indent_level) const
    {
        return prefix_of(is, indent_level)
                + '\n' + visit(is->condition, indent_level+1)
                + visit_nodes(is->then_stmts, indent_level+1)
                + visit_nodes_with_predicate(is->elseif_stmts_list,
                        [this, indent_level](auto const& cond_and_then_stmts){
                            return visit(cond_and_then_stmts.first, indent_level+1)
                                + visit_nodes(cond_and_then_stmts.second, indent_level+1);
                        })
                + (is->maybe_else_stmts ? visit_nodes(*(is->maybe_else_stmts), indent_level+1) : "");
    }

    std::string visit(ast::node::postfix_if_stmt const& pis, size_t const indent_level) const
    {
        return prefix_of(pis, indent_level)
                + '\n' + visit(pis->body, indent_level+1)
                + '\n' + visit(pis->condition, indent_level+1);
    }

    std::string visit(ast::node::statement const& s, size_t const indent_level) const
    {
        return prefix_of(s, indent_level) + '\n' + visit_variant_node(s->value, indent_level+1);
    }

    // For terminal nodes
    template<class T, class = typename std::enable_if<ast::is_node<T>::value>::type>
    std::string visit(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level);
    }
};

} // namespace detail

std::string stringize_ast(ast::ast const& ast)
{
    return detail::ast_stringizer{}.visit(ast.root, 0);
}

} // namespace helper
} // namespace dachs
