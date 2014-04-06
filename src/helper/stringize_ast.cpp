#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <type_traits>
#include <cstddef>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
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
        return "\033[93m" + std::string(level, '.') + "\033[0m";
    }

    template<class T>
    std::string prefix_of(std::shared_ptr<T> const& p, size_t const indent_level) const
    {
        return indent(indent_level) + "\033[92m" + p->to_string() + "\033[90m" + (boost::format(" (line:%1%, col:%2%, length:%3%)") % p->line % p->col % p->length).str() + "\033[0m";
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
                          return indent(indent_level+1) + "\033[92mOPERATOR: " + ast::to_string(op_and_rhs.first) + "\033[0m"
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

    std::string visit(ast::node::parameter const& p, size_t const indent_level) const
    {
        return prefix_of(p, indent_level)
                + '\n' + visit(p->name, indent_level+1)
                + (p->type ? '\n' + visit(*(p->type), indent_level+1) : "");
    }

    std::string visit(ast::node::object_construct const& oc, size_t const indent_level) const
    {
        return prefix_of(oc, indent_level)
            + '\n' + visit(oc->type, indent_level+1)
            + (oc->args ? '\n' + visit(*(oc->args), indent_level+1) : "");
    }

    std::string visit(ast::node::var_ref const& vr, size_t const indent_level) const
    {
        return prefix_of(vr, indent_level) + '\n' + visit(vr->name, indent_level+1);
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

    std::string visit(ast::node::primary_type const& pt, size_t const indent_level) const
    {
        return prefix_of(pt, indent_level) + '\n'
                + visit_variant_node(pt->value, indent_level+1);
    }

    std::string visit(ast::node::array_type const& at, size_t const indent_level) const
    {
        return prefix_of(at, indent_level) + '\n' + visit(at->elem_type, indent_level+1);
    }

    std::string visit(ast::node::tuple_type const& tt, size_t const indent_level) const
    {
        return prefix_of(tt, indent_level)
            + visit_nodes(tt->arg_types, indent_level+1);
    }

    std::string visit(ast::node::compound_type const& ct, size_t const indent_level) const
    {
        return prefix_of(ct, indent_level) + '\n'
                + visit_variant_node(ct->value, indent_level+1);
    }

    std::string visit(ast::node::qualified_type const& qt, size_t const indent_level) const
    {
        return prefix_of(qt, indent_level)
                + '\n' + visit(qt->type, indent_level+1);
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

    std::string visit(ast::node::if_expr const& ie, size_t const indent_level) const
    {
        return prefix_of(ie, indent_level)
                + '\n' + visit(ie->condition_expr, indent_level+1)
                + '\n' + visit(ie->then_expr, indent_level+1)
                + '\n' + visit(ie->else_expr, indent_level+1);
    }

    std::string visit(ast::node::compound_expr const& e, size_t const indent_level) const
    {
        return prefix_of(e, indent_level) + '\n'
               + visit_variant_node(e->child_expr, indent_level+1)
               + visit_optional_node(e->maybe_type, indent_level+1);
    }

    std::string visit(ast::node::assignment_stmt const& as, size_t const indent_level) const
    {
        return prefix_of(as, indent_level)
               + visit_nodes(as->assignees, indent_level+1)
               + '\n' + indent(indent_level+1) + "\033[92mASSIGN_OPERATOR: " + ast::to_string(as->assign_op) + "\033[0m"
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

    std::string visit(ast::node::case_stmt const& cs, size_t const indent_level) const
    {
        return prefix_of(cs, indent_level)
                + visit_nodes_with_predicate(cs->when_stmts_list,
                        [this, indent_level](auto const& cond_and_when_stmts){
                            return visit(cond_and_when_stmts.first, indent_level+1)
                                + visit_nodes(cond_and_when_stmts.second, indent_level+1);
                        })
                + (cs->maybe_else_stmts ? visit_nodes(*(cs->maybe_else_stmts), indent_level+1) : "");
    }

    std::string visit(ast::node::switch_stmt const& ss, size_t const indent_level) const
    {
        return prefix_of(ss, indent_level)
                + '\n' + visit(ss->target_expr, indent_level+1)
                + visit_nodes_with_predicate(ss->when_stmts_list,
                        [this, indent_level](auto const& cond_and_when_stmts){
                            return visit(cond_and_when_stmts.first, indent_level+1)
                                + visit_nodes(cond_and_when_stmts.second, indent_level+1);
                        })
                + (ss->maybe_else_stmts ? visit_nodes(*(ss->maybe_else_stmts), indent_level+1) : "");
    }

    std::string visit(ast::node::return_stmt const& rs, size_t const indent_level) const
    {
        return prefix_of(rs, indent_level) + visit_nodes(rs->ret_exprs, indent_level+1);
    }

    std::string visit(ast::node::for_stmt const& fs, size_t const indent_level) const
    {
        return prefix_of(fs, indent_level)
                + visit_nodes(fs->iter_vars, indent_level+1)
                + '\n' + visit(fs->range_expr, indent_level+1)
                + visit_nodes(fs->body_stmts, indent_level+1);
    }

    std::string visit(ast::node::while_stmt const& ws, size_t const indent_level) const
    {
        return prefix_of(ws, indent_level)
                + '\n' + visit(ws->condition, indent_level+1)
                + visit_nodes(ws->body_stmts, indent_level+1);
    }

    std::string visit(ast::node::postfix_if_stmt const& pis, size_t const indent_level) const
    {
        return prefix_of(pis, indent_level)
                + '\n' + visit(pis->body, indent_level+1)
                + '\n' + visit(pis->condition, indent_level+1);
    }

    std::string visit(ast::node::compound_stmt const& s, size_t const indent_level) const
    {
        return prefix_of(s, indent_level) + '\n' + visit_variant_node(s->value, indent_level+1);
    }

    std::string visit(ast::node::variable_decl const& vd, size_t const indent_level) const
    {
        return prefix_of(vd, indent_level) +
            '\n' + visit(vd->name, indent_level+1)
            + (vd->maybe_type ? '\n' + visit(*(vd->maybe_type), indent_level+1) : "");
    }

    std::string visit(ast::node::initialize_stmt const& vds, size_t const indent_level) const
    {
        return prefix_of(vds, indent_level)
            + visit_nodes(vds->var_decls, indent_level+1)
            + (vds->maybe_rhs_exprs ? visit_nodes(*(vds->maybe_rhs_exprs), indent_level+1) : "");
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
