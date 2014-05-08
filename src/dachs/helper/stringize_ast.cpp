#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <type_traits>
#include <functional>
#include <cstddef>
#include <cstring>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <boost/range/numeric.hpp>

#include "dachs/helper/stringize_ast.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace helper {
namespace detail {

using std::size_t;
using boost::adaptors::transformed;
using boost::adaptors::sliced;
using std::placeholders::_1;

struct to_string : public boost::static_visitor<std::string> {
    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const noexcept
    {
        static_assert(ast::traits::is_node<T>::value, "ast_stringizer: visit a object which isn't AST node.");
        return p->to_string();
    }

    template<class T, class = typename std::enable_if<!is_shared_ptr<T>::value>::type>
    std::string operator()(T const& value) const noexcept
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
    std::string operator()(std::shared_ptr<T> const& p) const noexcept
    {
        return visitor.visit(p, indent, lead);
    }
};

template<class String>
class ast_stringizer {
    template<class T>
    String prefix_of(std::shared_ptr<T> const& p, String const& indent) const noexcept
    {
        return c.yellow(indent+"|\n"+indent+"|--", false)
            + c.green(p->to_string(), false)
            + c.gray((boost::format(" (line:%1%, col:%2%, len:%3%)") % p->line % p->col % p->length).str());
    }

    template<class... Args>
    String visit_variant_node(boost::variant<Args...> const& v, String const& indent, char const* const lead) const noexcept
    {
        return boost::apply_visitor(node_variant_visitor<ast_stringizer>{*this, indent, lead}, v);
    }

    template<class T>
    String visit_optional_node(boost::optional<T> const& o, String const& indent, char const* const lead) const noexcept
    {
        return (o ? '\n' + visit(*o, indent, lead) : "");
    }

    template<class NodeRange, class Pred>
    String visit_nodes_with_predicate(NodeRange const& ptrs, Pred const& predicate, bool const is_last) const noexcept
    {
        if (ptrs.empty()) {
            return "";
        }

        auto combine = [](auto const& acc, auto const& item){
                           return acc + '\n' + item;
                       };

        if (!is_last) {
            return boost::accumulate(ptrs | transformed(std::bind(predicate, _1, "|  "))
                                    , String{}
                                    , combine);
        } else {
            return boost::accumulate(ptrs | sliced(0, ptrs.size()-1) | transformed(std::bind(predicate, _1, "|  "))
                                    , String{}
                                    , combine)
                   + '\n' + predicate(ptrs.back(), "   ");
        }
    }

    template<class NodePtrs>
    String visit_nodes(NodePtrs const& ptrs, String const& indent, bool const is_last) const noexcept
    {
        return visit_nodes_with_predicate(ptrs,
                                          [this, indent](auto const& p, auto const lead){
                                              return visit(p, indent, lead);
                                          },
                                          is_last);
    }

    template<class NodePtrs>
    String visit_node_variants(NodePtrs const& ptrs, String const& indent, bool const is_last) const noexcept
    {
        return visit_nodes_with_predicate(
                    ptrs,
                    [this, indent](auto const& p, auto const lead){
                        return visit_variant_node(p, indent, lead);
                    }, 
                    is_last);
    }

    template<class BinaryOperatorNodePtr>
    String visit_binary_operator_ptr(BinaryOperatorNodePtr const& p, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(p, indent) + '\n'
                + visit(p->lhs, indent+lead, (p->rhss.empty() ? "   " : "|  "))
                + visit_nodes_with_predicate(
                      p->rhss,
                      [this, indent, lead](auto const& op_and_rhs, auto const l) {
                          return c.yellow(indent+lead+"|\n"+indent+lead+"|--", false)
                                + c.green("OPERATOR: " + ast::symbol::to_string(op_and_rhs.first))
                              + '\n' + visit(op_and_rhs.second, indent+lead, l);
                      }, true);
    }

    colorizer<String> c;

public:

    explicit ast_stringizer(bool const colorful)
        : c(colorful)
    {}

    String visit(ast::node::program const& p, String const& indent) const noexcept
    {
        return prefix_of(p, indent) + visit_nodes(p->inu, indent + "   ", true);
    }

    String visit(ast::node::array_literal const& al, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(al, indent) + visit_nodes(al->element_exprs, indent+lead, true);
    }

    String visit(ast::node::tuple_literal const& tl, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tl, indent) + visit_nodes(tl->element_exprs, indent+lead, true);
    }

    String visit(ast::node::dict_literal const& ml, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ml, indent)
                + visit_nodes_with_predicate(ml->value,
                        [this, indent, lead](auto const& key_value, auto const l){
                            return visit(key_value.first, indent+lead, "|  ")
                                + '\n' + visit(key_value.second, indent+lead, l);
                        }, true);
    }

    String visit(ast::node::literal const& l, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(l, indent) + '\n' + visit_variant_node(l->value, indent+lead, "   ");
    }

    String visit(ast::node::parameter const& p, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(p, indent)
                + visit_optional_node(p->param_type, indent+lead, "   ");
    }

    String visit(ast::node::object_construct const& oc, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(oc, indent)
            + '\n' + visit(oc->obj_type, indent+lead, (oc->args.empty() ? "   " : "|  "))
            + visit_nodes(oc->args, indent+lead, "   ");
    }

    String visit(ast::node::primary_expr const& pe, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pe, indent) + '\n' + visit_variant_node(pe->value, indent+lead, "   ");
    }

    String visit(ast::node::index_access const& ia, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ia, indent) + '\n' + visit(ia->index_expr, indent+lead, "   ");
    }

    String visit(ast::node::function_call const& fc, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fc, indent) + visit_nodes(fc->args, indent+lead, true);
    }

    String visit(ast::node::postfix_expr const& pe, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pe, indent)
            + visit_node_variants(pe->postfixes, indent+lead, false) + '\n'
            + visit(pe->prefix, indent+lead, "   ");
    }

    String visit(ast::node::unary_expr const& ue, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ue, indent) + '\n'
                + visit(ue->expr, indent+lead, "   ");
    }

    String visit(ast::node::primary_type const& tt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tt, indent)
            + (tt->instantiated_templates ? visit_nodes(*(tt->instantiated_templates), indent+lead, true) : "");
    }

    String visit(ast::node::nested_type const& pt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pt, indent) + '\n'
                + visit_variant_node(pt->value, indent+lead, "   ");
    }

    String visit(ast::node::array_type const& at, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(at, indent) + '\n' + visit(at->elem_type, indent+lead, "   ");
    }

    String visit(ast::node::dict_type const& mt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(mt, indent)
            + '\n' + visit(mt->key_type, indent+lead, "|  ")
            + '\n' + visit(mt->value_type, indent+lead, "   ");
    }

    String visit(ast::node::tuple_type const& tt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tt, indent)
            + visit_nodes(tt->arg_types, indent+lead, true);
    }

    String visit(ast::node::func_type const& ft, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ft, indent)
            + visit_nodes(ft->arg_types, indent+lead, false)
            + '\n' + visit(ft->ret_type, indent+lead, "   ");
    }

    String visit(ast::node::proc_type const& pt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pt, indent)
            + visit_nodes(pt->arg_types, indent+lead, true);
    }

    String visit(ast::node::compound_type const& ct, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ct, indent) + '\n'
                + visit_variant_node(ct->value, indent+lead, "   ");
    }

    String visit(ast::node::qualified_type const& qt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(qt, indent)
                + '\n' + visit(qt->type, indent+lead, "   ");
    }

    String visit(ast::node::cast_expr const& ce, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ce, indent)
                + visit_nodes(ce->dest_types, indent+lead, false) + '\n'
                + visit(ce->source_expr, indent+lead, "   ");
    }

    String visit(ast::node::mult_expr const& me, String const& indent, char const* const lead) const noexcept
    {
        return visit_binary_operator_ptr(me, indent, lead);
    }

    String visit(ast::node::additive_expr const& ae, String const& indent, char const* const lead) const noexcept
    {
        return visit_binary_operator_ptr(ae, indent, lead);
    }

    String visit(ast::node::shift_expr const& se, String const& indent, char const* const lead) const noexcept
    {
        return visit_binary_operator_ptr(se, indent, lead);
    }

    String visit(ast::node::relational_expr const& re, String const& indent, char const* const lead) const noexcept
    {
        return visit_binary_operator_ptr(re, indent, lead);
    }

    String visit(ast::node::equality_expr const& ee, String const& indent, char const* const lead) const noexcept
    {
        return visit_binary_operator_ptr(ee, indent, lead);
    }

    String visit(ast::node::and_expr const& ae, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ae, indent)
            + '\n' + visit(ae->lhs, indent+lead, ae->rhss.empty() ? "   " : "|  ")
            + visit_nodes(ae->rhss, indent+lead, true);
    }

    String visit(ast::node::xor_expr const& xe, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(xe, indent)
            + '\n' + visit(xe->lhs, indent+lead, xe->rhss.empty() ? "   " : "|  ")
            + visit_nodes(xe->rhss, indent+lead, true);
    }

    String visit(ast::node::or_expr const& oe, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(oe, indent)
            + '\n' + visit(oe->lhs, indent+lead, oe->rhss.empty() ? "   " : "|  ")
            + visit_nodes(oe->rhss, indent+lead, true);
    }

    String visit(ast::node::logical_and_expr const& lae, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(lae, indent)
            + '\n' + visit(lae->lhs, indent+lead, lae->rhss.empty() ? "   " : "|  ")
            + visit_nodes(lae->rhss, indent+lead, true);
    }

    String visit(ast::node::logical_or_expr const& loe, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(loe, indent)
            + '\n' + visit(loe->lhs, indent+lead, loe->rhss.empty() ? "   " : "|  ")
            + visit_nodes(loe->rhss, indent+lead, true);
    }

    String visit(ast::node::range_expr const& re, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(re, indent)
            + '\n' + visit(re->lhs, indent+lead, re->has_range() ? "|  " : "   ")
            + (re->has_range() ? '\n' + visit((*re->maybe_rhs).second, indent+lead, "   ") : "");
    }

    String visit(ast::node::if_expr const& ie, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ie, indent)
                + '\n' + visit(ie->condition_expr, indent+lead, "|  ")
                + '\n' + visit(ie->then_expr, indent+lead, "|  ")
                + '\n' + visit(ie->else_expr, indent+lead, "   ");
    }

    String visit(ast::node::compound_expr const& e, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(e, indent) + '\n'
               + visit_variant_node(e->child_expr, indent+lead, e->maybe_type ? "|  " : "   ")
               + visit_optional_node(e->maybe_type, indent+lead, "   ");
    }

    String visit(ast::node::assignment_stmt const& as, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(as, indent)
               + visit_nodes(as->assignees, indent+lead, false)
               + '\n' + c.yellow(indent+lead+"|\n"+indent+lead+"|--")
               + c.green("ASSIGN_OPERATOR: " + ast::symbol::to_string(as->assign_op))
               + visit_nodes(as->rhs_exprs, indent+lead, true);
    }

    String visit(ast::node::if_stmt const& is, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(is, indent)
                + '\n' + visit(is->condition, indent+lead, "|  ")
                + '\n' + visit(is->then_stmts, indent+lead, is->elseif_stmts_list.empty() && !is->maybe_else_stmts ? "   " : "|  ")
                + visit_nodes_with_predicate(is->elseif_stmts_list,
                        [this, indent, lead](auto const& cond_and_then_stmts, auto const l){
                            return visit(cond_and_then_stmts.first, indent+lead, "|  ")
                                + '\n' + visit(cond_and_then_stmts.second, indent+lead, l);
                        }, !is->maybe_else_stmts)
                + (is->maybe_else_stmts ? '\n' + visit(*(is->maybe_else_stmts), indent+lead, "   ") : "");
    }

    String visit(ast::node::case_stmt const& cs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(cs, indent)
                + visit_nodes_with_predicate(cs->when_stmts_list,
                        [this, indent, lead](auto const& cond_and_when_stmts, auto const l){
                            return visit(cond_and_when_stmts.first, indent+lead, "|  ")
                                + '\n' + visit(cond_and_when_stmts.second, indent+lead, l);
                        }, !cs->maybe_else_stmts)
                + (cs->maybe_else_stmts ? visit(*(cs->maybe_else_stmts), indent+lead, "   ") : "");
    }

    String visit(ast::node::switch_stmt const& ss, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ss, indent)
                + '\n' + visit(ss->target_expr, indent+lead, ss->when_stmts_list.empty() && !ss->maybe_else_stmts ? "   " : "|  ")
                + visit_nodes_with_predicate(ss->when_stmts_list,
                        [this, indent, lead](auto const& cond_and_when_stmts, auto const l){
                            return visit_nodes(cond_and_when_stmts.first, indent+lead, false)
                                + visit(cond_and_when_stmts.second, indent+lead, l);
                        }, !ss->maybe_else_stmts)
                + (ss->maybe_else_stmts ? visit(*(ss->maybe_else_stmts), indent+lead, "   ") : "");
    }

    String visit(ast::node::return_stmt const& rs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(rs, indent) + visit_nodes(rs->ret_exprs, indent+lead, true);
    }

    String visit(ast::node::for_stmt const& fs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fs, indent)
                + visit_nodes(fs->iter_vars, indent+lead, false)
                + '\n' + visit(fs->range_expr, indent+lead, "|  ")
                + '\n' + visit(fs->body_stmts, indent+lead, "   ");
    }

    String visit(ast::node::while_stmt const& ws, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ws, indent)
                + '\n' + visit(ws->condition, indent+lead, "|  ")
                + '\n' + visit(ws->body_stmts, indent+lead, "   ");
    }

    String visit(ast::node::postfix_if_stmt const& pis, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pis, indent)
                + '\n' + visit_variant_node(pis->body, indent+lead, "|  ")
                + '\n' + visit(pis->condition, indent+lead, "   ");
    }

    String visit(ast::node::variable_decl const& vd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(vd, indent)
            + (vd->maybe_type ? '\n' + visit(*(vd->maybe_type), indent+lead, "   ") : "");
    }

    String visit(ast::node::initialize_stmt const& vds, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(vds, indent)
            + visit_nodes(vds->var_decls, indent+lead, !vds->maybe_rhs_exprs)
            + (vds->maybe_rhs_exprs ? visit_nodes(*(vds->maybe_rhs_exprs), indent+lead, true) : "");
    }

    String visit(ast::node::compound_stmt const& s, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(s, indent) + '\n' + visit_variant_node(s->value, indent+lead, "   ");
    }

    String visit(ast::node::statement_block const& sb, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(sb, indent) + visit_nodes(sb->value, indent+lead, true);
    }

    String visit(ast::node::function_definition const& fd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fd, indent)
            + visit_nodes(fd->params, indent+lead, false)
            + visit_optional_node(fd->return_type, indent+lead, "|  ")
            + '\n' + visit(fd->body, indent+lead, fd->ensure_body ? "|  " : "   ")
            + visit_optional_node(fd->ensure_body, indent+lead, "   ");
    }

    String visit(ast::node::constant_decl const& cd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(cd, indent)
            + (cd->maybe_type ? '\n' + visit(*(cd->maybe_type), indent+lead, "   ") : "");
    }

    String visit(ast::node::constant_definition const& cd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(cd, indent)
            + visit_nodes(cd->const_decls, indent+lead, cd->initializers.empty())
            + visit_nodes(cd->initializers, indent+lead, true);
    }

    String visit(ast::node::global_definition const& gd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(gd, indent)
            + '\n' + visit_variant_node(gd->value, indent+lead, "   ");
    }

    // For terminal nodes
    template<class T>
    String visit(std::shared_ptr<T> const& p, String const& indent, char const* const) const noexcept
    {
        static_assert(ast::traits::is_node<T>::value, "ast_stringizer: visit a object which isn't AST node.");
        return prefix_of(p, indent);
    }
};

} // namespace detail

std::string stringize_ast(ast::ast const& ast, bool const colorful)
{
    return detail::ast_stringizer<std::string>{colorful}.visit(ast.root, "");
}

} // namespace helper
} // namespace dachs
