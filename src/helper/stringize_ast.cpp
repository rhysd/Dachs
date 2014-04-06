#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <type_traits>
#include <functional>
#include <cstddef>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/numeric.hpp>

#include "stringize_ast.hpp"
#include "helper/variant.hpp"

namespace dachs {
namespace helper {
namespace detail {

using std::size_t;
using boost::adaptors::transformed;
using boost::adaptors::sliced;
using std::placeholders::_1;

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
    std::string const& indent;
    char const* const lead;

    node_variant_visitor(V const& v, std::string const& i, char const* const l) : visitor(v), indent(i), lead(l)
    {}

    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const
    {
        return visitor.visit(p, indent, lead);
    }
};

class ast_stringizer {
    std::string yellow(std::string const& indent) const
    {
        return "\033[93m" + indent + "\033[0m";
    }

    template<class T>
    std::string prefix_of(std::shared_ptr<T> const& p, std::string const& indent) const
    {
        return yellow(indent+'|') + '\n' + yellow(indent+"|--")
            + "\033[92m" + p->to_string()
            + "\033[90m" + (boost::format(" (line:%1%, col:%2%, length:%3%)") % p->line % p->col % p->length).str()
            + "\033[0m";
    }

    template<class... Args>
    std::string visit_variant_node(boost::variant<Args...> const& v, std::string const& indent, char const* const lead) const
    {
        return boost::apply_visitor(node_variant_visitor<ast_stringizer>{*this, indent, lead}, v);
    }

    template<class T>
    std::string visit_optional_node(boost::optional<T> const& o, std::string const& indent, char const* const lead) const
    {
        return (o ? '\n' + visit(*o, indent, lead) : "");
    }

    template<class NodeRange, class Pred>
    std::string visit_nodes_with_predicate(NodeRange const& ptrs, Pred const& predicate, bool const is_last) const
    {
        if (ptrs.empty()) {
            return "";
        }

        auto combine = [](auto const& acc, auto const& item){
                           return acc + '\n' + item;
                       };

        if (!is_last) {
            return boost::accumulate(ptrs | transformed(std::bind(predicate, _1, "|  "))
                                    , std::string{}
                                    , combine);
        } else {
            return boost::accumulate(ptrs | sliced(0, ptrs.size()-1) | transformed(std::bind(predicate, _1, "|  "))
                                    , std::string{}
                                    , combine)
                   + '\n' + predicate(ptrs.back(), "   ");
        }
    }

    template<class NodePtrs>
    std::string visit_nodes(NodePtrs const& ptrs, std::string const& indent, bool const is_last) const
    {
        return visit_nodes_with_predicate(ptrs,
                                          [this, indent](auto const& p, auto const lead){
                                              return visit(p, indent, lead);
                                          },
                                          is_last);
    }

    template<class NodePtrs>
    std::string visit_node_variants(NodePtrs const& ptrs, std::string const& indent, bool const is_last) const
    {
        return visit_nodes_with_predicate(
                    ptrs,
                    [this, indent](auto const& p, auto const lead){
                        return visit_variant_node(p, indent, lead);
                    }, 
                    is_last);
    }

    template<class BinaryOperatorNodePtr>
    std::string visit_binary_operator_ptr(BinaryOperatorNodePtr const& p, std::string const& indent, char const* const lead) const
    {
        return prefix_of(p, indent) + '\n'
                + visit(p->lhs, indent+lead, (p->rhss.empty() ? "   " : "|  "))
                + visit_nodes_with_predicate(
                      p->rhss,
                      [this, indent, lead](auto const& op_and_rhs, auto const l) {
                          return yellow(indent+lead+'|') + '\n' + yellow(indent+lead+"|--") + "\033[92mOPERATOR: " + ast::to_string(op_and_rhs.first) + "\033[0m"
                              + '\n' + visit(op_and_rhs.second, indent+lead, l);
                      }, true);
    }

public:

    std::string visit(ast::node::program const& p, std::string const& indent) const
    {
        return prefix_of(p, indent) + visit_node_variants(p->inu, indent, true);
    }

    std::string visit(ast::node::array_literal const& al, std::string const& indent, char const* const lead) const
    {
        return prefix_of(al, indent) + visit_nodes(al->element_exprs, indent+lead, true);
    }

    std::string visit(ast::node::tuple_literal const& tl, std::string const& indent, char const* const lead) const
    {
        return prefix_of(tl, indent) + visit_nodes(tl->element_exprs, indent+lead, true);
    }

    std::string visit(ast::node::literal const& l, std::string const& indent, char const* const lead) const
    {
        return prefix_of(l, indent) + '\n' + visit_variant_node(l->value, indent+lead, "   ");
    }

    std::string visit(ast::node::parameter const& p, std::string const& indent, char const* const lead) const
    {
        return prefix_of(p, indent)
                + '\n' + visit(p->name, indent+lead, (p->type ? "|  " : "   "))
                + (p->type ? '\n' + visit(*(p->type), indent+lead, "   ") : "");
    }

    std::string visit(ast::node::object_construct const& oc, std::string const& indent, char const* const lead) const
    {
        return prefix_of(oc, indent)
            + '\n' + visit(oc->type, indent+lead, (oc->args.empty() ? "   " : "|  "))
            + visit_nodes(oc->args, indent+lead, "   ");
    }

    std::string visit(ast::node::var_ref const& vr, std::string const& indent, char const* const lead) const
    {
        return prefix_of(vr, indent) + '\n' + visit(vr->name, indent+lead, "   ");
    }

    std::string visit(ast::node::primary_expr const& pe, std::string const& indent, char const* const lead) const
    {
        return prefix_of(pe, indent) + '\n' + visit_variant_node(pe->value, indent+lead, "   ");
    }

    std::string visit(ast::node::index_access const& ia, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ia, indent) + '\n' + visit(ia->index_expr, indent+lead, "   ");
    }

    std::string visit(ast::node::member_access const& ma, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ma, indent) + '\n'
                + visit(ma->member_name, indent+lead, "   ");
    }

    std::string visit(ast::node::function_call const& fc, std::string const& indent, char const* const lead) const
    {
        return prefix_of(fc, indent) + visit_nodes(fc->args, indent+lead, true);
    }

    std::string visit(ast::node::postfix_expr const& pe, std::string const& indent, char const* const lead) const
    {
        return prefix_of(pe, indent)
            + visit_node_variants(pe->postfixes, indent+lead, false) + '\n'
            + visit(pe->prefix, indent+lead, "   ");
    }

    std::string visit(ast::node::unary_expr const& ue, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ue, indent) + '\n'
                + visit(ue->expr, indent+lead, "   ");
    }

    std::string visit(ast::node::primary_type const& pt, std::string const& indent, char const* const lead) const
    {
        return prefix_of(pt, indent) + '\n'
                + visit_variant_node(pt->value, indent+lead, "   ");
    }

    std::string visit(ast::node::array_type const& at, std::string const& indent, char const* const lead) const
    {
        return prefix_of(at, indent) + '\n' + visit(at->elem_type, indent+lead, "   ");
    }

    std::string visit(ast::node::tuple_type const& tt, std::string const& indent, char const* const lead) const
    {
        return prefix_of(tt, indent)
            + visit_nodes(tt->arg_types, indent+lead, true);
    }

    std::string visit(ast::node::compound_type const& ct, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ct, indent) + '\n'
                + visit_variant_node(ct->value, indent+lead, "   ");
    }

    std::string visit(ast::node::qualified_type const& qt, std::string const& indent, char const* const lead) const
    {
        return prefix_of(qt, indent)
                + '\n' + visit(qt->type, indent+lead, "   ");
    }

    std::string visit(ast::node::cast_expr const& ce, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ce, indent)
                + visit_nodes(ce->dest_types, indent+lead, false) + '\n'
                + visit(ce->source_expr, indent+lead, "   ");
    }

    std::string visit(ast::node::mult_expr const& me, std::string const& indent, char const* const lead) const
    {
        return visit_binary_operator_ptr(me, indent, lead);
    }

    std::string visit(ast::node::additive_expr const& ae, std::string const& indent, char const* const lead) const
    {
        return visit_binary_operator_ptr(ae, indent, lead);
    }

    std::string visit(ast::node::shift_expr const& se, std::string const& indent, char const* const lead) const
    {
        return visit_binary_operator_ptr(se, indent, lead);
    }

    std::string visit(ast::node::relational_expr const& re, std::string const& indent, char const* const lead) const
    {
        return visit_binary_operator_ptr(re, indent, lead);
    }

    std::string visit(ast::node::equality_expr const& ee, std::string const& indent, char const* const lead) const
    {
        return visit_binary_operator_ptr(ee, indent, lead);
    }

    std::string visit(ast::node::and_expr const& ae, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ae, indent) + visit_nodes(ae->exprs, indent+lead, true);
    }

    std::string visit(ast::node::xor_expr const& xe, std::string const& indent, char const* const lead) const
    {
        return prefix_of(xe, indent) + visit_nodes(xe->exprs, indent+lead, true);
    }

    std::string visit(ast::node::or_expr const& oe, std::string const& indent, char const* const lead) const
    {
        return prefix_of(oe, indent) + visit_nodes(oe->exprs, indent+lead, true);
    }

    std::string visit(ast::node::logical_and_expr const& lae, std::string const& indent, char const* const lead) const
    {
        return prefix_of(lae, indent) + visit_nodes(lae->exprs, indent+lead, true);
    }

    std::string visit(ast::node::logical_or_expr const& loe, std::string const& indent, char const* const lead) const
    {
        return prefix_of(loe, indent) + visit_nodes(loe->exprs, indent+lead, true);
    }

    std::string visit(ast::node::if_expr const& ie, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ie, indent)
                + '\n' + visit(ie->condition_expr, indent+lead, "|  ")
                + '\n' + visit(ie->then_expr, indent+lead, "|  ")
                + '\n' + visit(ie->else_expr, indent+lead, "   ");
    }

    std::string visit(ast::node::compound_expr const& e, std::string const& indent, char const* const lead) const
    {
        return prefix_of(e, indent) + '\n'
               + visit_variant_node(e->child_expr, indent+lead, e->maybe_type ? "|  " : "   ")
               + visit_optional_node(e->maybe_type, indent+lead, "   ");
    }

    std::string visit(ast::node::assignment_stmt const& as, std::string const& indent, char const* const lead) const
    {
        return prefix_of(as, indent)
               + visit_nodes(as->assignees, indent+lead, false)
               + '\n' + yellow(indent+lead+'|')
               + '\n' + yellow(indent+lead+"|--") + "\033[92mASSIGN_OPERATOR: " + ast::to_string(as->assign_op) + "\033[0m"
               + visit_nodes(as->rhs_exprs, indent+lead, true);
    }

    std::string visit(ast::node::if_stmt const& is, std::string const& indent, char const* const lead) const
    {
        return prefix_of(is, indent)
                + '\n' + visit(is->condition, indent+lead, "|  ")
                + visit_nodes(is->then_stmts, indent+lead, false)
                + visit_nodes_with_predicate(is->elseif_stmts_list,
                        [this, indent, lead](auto const& cond_and_then_stmts, auto const l){
                            return visit(cond_and_then_stmts.first, indent+lead, l)
                                + visit_nodes(cond_and_then_stmts.second, indent+lead, std::string{l}=="   ");
                        }, !is->maybe_else_stmts)
                + (is->maybe_else_stmts ? visit_nodes(*(is->maybe_else_stmts), indent+lead, true) : "");
    }

    std::string visit(ast::node::case_stmt const& cs, std::string const& indent, char const* const lead) const
    {
        return prefix_of(cs, indent)
                + visit_nodes_with_predicate(cs->when_stmts_list,
                        [this, indent, lead](auto const& cond_and_when_stmts, auto const l){
                            return visit(cond_and_when_stmts.first, indent+lead, l)
                                + visit_nodes(cond_and_when_stmts.second, indent+lead, std::string{l}=="   ");
                        }, !cs->maybe_else_stmts)
                + (cs->maybe_else_stmts ? visit_nodes(*(cs->maybe_else_stmts), indent+lead, true) : "");
    }

    std::string visit(ast::node::switch_stmt const& ss, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ss, indent)
                + '\n' + visit(ss->target_expr, indent+lead, "|  ")
                + visit_nodes_with_predicate(ss->when_stmts_list,
                        [this, indent, lead](auto const& cond_and_when_stmts, auto const l){
                            return visit(cond_and_when_stmts.first, indent+lead, l)
                                + visit_nodes(cond_and_when_stmts.second, indent+lead, std::string{l}=="   ");
                        }, !ss->maybe_else_stmts)
                + (ss->maybe_else_stmts ? visit_nodes(*(ss->maybe_else_stmts), indent+lead, true) : "");
    }

    std::string visit(ast::node::return_stmt const& rs, std::string const& indent, char const* const lead) const
    {
        return prefix_of(rs, indent) + visit_nodes(rs->ret_exprs, indent+lead, true);
    }

    std::string visit(ast::node::for_stmt const& fs, std::string const& indent, char const* const lead) const
    {
        return prefix_of(fs, indent)
                + visit_nodes(fs->iter_vars, indent+lead, false)
                + '\n' + visit(fs->range_expr, indent+lead, "|  ")
                + visit_nodes(fs->body_stmts, indent+lead, true);
    }

    std::string visit(ast::node::while_stmt const& ws, std::string const& indent, char const* const lead) const
    {
        return prefix_of(ws, indent)
                + '\n' + visit(ws->condition, indent+lead, "|  ")
                + visit_nodes(ws->body_stmts, indent+lead, true);
    }

    std::string visit(ast::node::postfix_if_stmt const& pis, std::string const& indent, char const* const lead) const
    {
        return prefix_of(pis, indent)
                + '\n' + visit(pis->body, indent+lead, "|  ")
                + '\n' + visit(pis->condition, indent+lead, "   ");
    }

    std::string visit(ast::node::compound_stmt const& s, std::string const& indent, char const* const lead) const
    {
        return prefix_of(s, indent) + '\n' + visit_variant_node(s->value, indent+lead, "   ");
    }

    std::string visit(ast::node::variable_decl const& vd, std::string const& indent, char const* const lead) const
    {
        return prefix_of(vd, indent) +
            '\n' + visit(vd->name, indent+lead, (vd->maybe_type ? "|  " : "   "))
            + (vd->maybe_type ? '\n' + visit(*(vd->maybe_type), indent+lead, "   ") : "");
    }

    std::string visit(ast::node::initialize_stmt const& vds, std::string const& indent, char const* const lead) const
    {
        return prefix_of(vds, indent)
            + visit_nodes(vds->var_decls, indent+lead, !vds->maybe_rhs_exprs)
            + (vds->maybe_rhs_exprs ? visit_nodes(*(vds->maybe_rhs_exprs), indent+lead, true) : "");
    }

    std::string visit(ast::node::function_definition const& fd, std::string const& indent, char const* const lead) const
    {
        return prefix_of(fd, indent)
            + visit_nodes(fd->params, indent+lead, false)
            + (fd->return_type ? '\n' + visit(*(fd->return_type), indent+lead, "| ") : "")
            + visit_nodes(fd->body, indent+lead, true);
    }

    std::string visit(ast::node::procedure_definition const& pd, std::string const& indent, char const* const lead) const
    {
        return prefix_of(pd, indent)
            + visit_nodes(pd->params, indent+lead, false)
            + visit_nodes(pd->body, indent+lead, true);
    }

    // For terminal nodes
    template<class T, class = typename std::enable_if<ast::traits::is_node<T>::value>::type>
    std::string visit(std::shared_ptr<T> const& p, std::string const& indent, char const* const) const
    {
        return prefix_of(p, indent);
    }
};

} // namespace detail

std::string stringize_ast(ast::ast const& ast)
{
    return detail::ast_stringizer{}.visit(ast.root, "");
}

} // namespace helper
} // namespace dachs
