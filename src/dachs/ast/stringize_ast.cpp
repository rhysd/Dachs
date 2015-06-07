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
#include <boost/algorithm/string/join.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace ast {
namespace detail {

using std::size_t;
using boost::adaptors::transformed;
using boost::adaptors::sliced;
using std::placeholders::_1;

struct to_string : public boost::static_visitor<std::string> {
    template<class T>
    std::string operator()(std::shared_ptr<T> const& p) const noexcept
    {
        static_assert(traits::is_node<T>::value, "ast_stringizer: visit a object which isn't AST node.");
        return p->to_string();
    }

    template<class T, class = typename std::enable_if<!helper::is_shared_ptr<T>::value>::type>
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

    template<class... Args>
    std::string operator()(boost::variant<Args...> const& v) const noexcept
    {
        return visitor.visit(v, indent, lead);
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

    helper::colorizer c;

public:

    String visit(node::inu const& p, String const& indent) const noexcept
    {
        return prefix_of(p, indent)
            + visit_nodes(p->functions, indent + "   ", p->global_constants.empty() && p->classes.empty() && p->imports.empty())
            + visit_nodes(p->global_constants, indent + "   ", p->classes.empty() && p->imports.empty())
            + visit_nodes(p->classes, indent + "   ", p->imports.empty())
            + visit_nodes(p->imports, indent + "   ", true);
    }

    String visit(node::array_literal const& al, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(al, indent) + visit_nodes(al->element_exprs, indent+lead, true);
    }

    String visit(node::tuple_literal const& tl, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tl, indent) + visit_nodes(tl->element_exprs, indent+lead, true);
    }

    String visit(node::lambda_expr const& le, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(le, indent) + '\n' + visit(le->def, indent+lead, "   ");
    }

    String visit(node::dict_literal const& ml, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ml, indent)
                + visit_nodes_with_predicate(ml->value,
                        [this, indent, lead](auto const& key_value, auto const l){
                            return visit(key_value.first, indent+lead, "|  ")
                                + '\n' + visit(key_value.second, indent+lead, l);
                        }, true);
    }

    String visit(node::parameter const& p, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(p, indent)
                + visit_optional_node(p->param_type, indent+lead, "   ");
    }

    String visit(node::object_construct const& oc, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(oc, indent)
            + '\n' + visit(oc->obj_type, indent+lead, (oc->args.empty() ? "   " : "|  "))
            + visit_nodes(oc->args, indent+lead, true);
    }

    String visit(node::index_access const& ia, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ia, indent)
            + '\n' + visit(ia->child, indent+lead, "|  ")
            + '\n' + visit(ia->index_expr, indent+lead, "   ");
    }

    String visit(node::ufcs_invocation const& ui, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ui, indent)
            + '\n' + visit(ui->child, indent+lead, "   ");
    }

    String visit(node::func_invocation const& fc, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fc, indent)
            + '\n' + visit(fc->child, indent+lead, fc->args.empty() ? "   " : "|  ")
            + visit_nodes(fc->args, indent+lead, true);
    }

    String visit(node::unary_expr const& ue, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ue, indent) + '\n'
                + visit(ue->expr, indent+lead, "   ");
    }

    String visit(node::primary_type const& tt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tt, indent)
            + visit_nodes(tt->template_params, indent+lead, true);
    }

    String visit(node::array_type const& at, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(at, indent) + '\n'
            + visit_optional_node(at->elem_type, indent+lead, "   ");
    }

    String visit(node::dict_type const& mt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(mt, indent)
            + '\n' + visit(mt->key_type, indent+lead, "|  ")
            + '\n' + visit(mt->value_type, indent+lead, "   ");
    }

    String visit(node::pointer_type const& pt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pt, indent) + '\n'
            + visit_optional_node(pt->pointee_type, indent+lead, "   ");
    }

    String visit(node::typeof_type const& tt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tt, indent) + '\n'
            + visit(tt->expr, indent+lead, "   ");
    }

    String visit(node::tuple_type const& tt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(tt, indent)
            + visit_nodes(tt->arg_types, indent+lead, true);
    }

    String visit(node::func_type const& ft, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ft, indent)
            + visit_nodes(ft->arg_types, indent+lead, false)
            + visit_optional_node(ft->ret_type, indent+lead, "   ");
    }

    String visit(node::qualified_type const& qt, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(qt, indent)
                + '\n' + visit(qt->type, indent+lead, "   ");
    }

    String visit(node::cast_expr const& ce, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ce, indent)
                + '\n' + visit(ce->child, indent+lead, "|  ")
                + '\n' + visit(ce->cast_type, indent+lead, "   ");
    }

    String visit(node::binary_expr const& be, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(be, indent)
                + '\n' + visit(be->lhs, indent+lead, "|  ")
                + '\n' + visit(be->rhs, indent+lead, "   ");
    }

    String visit(node::block_expr const& be, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(be, indent)
            + '\n' + visit_nodes(be->stmts, indent+lead, false)
            + '\n' + visit(be->last_expr, indent+lead, "   ");
    }

    String visit(node::if_expr const& ie, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ie, indent)
                + '\n' + visit(ie->condition, indent+lead, "|  ")
                + '\n' + visit(ie->then_block, indent+lead, "|  ")
                + '\n' + visit_nodes_with_predicate(ie->elseif_block_list,
                        [this, indent, lead](auto const& block, auto const l){
                            return visit(block.first, indent+lead, "|  ")
                                + '\n' + visit(block.second, indent+lead, l);
                        }, false)
                + '\n' + visit(ie->else_block, indent+lead, "   ");
    }

    String visit(node::typed_expr const& te, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(te, indent)
                + '\n' + visit(te->child_expr, indent+lead, "|  ")
                + '\n' + visit(te->specified_type, indent+lead, "   ");
    }

    String visit(node::assignment_stmt const& as, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(as, indent)
               + visit_nodes(as->assignees, indent+lead, false)
               + '\n' + c.yellow(indent+lead+"|\n"+indent+lead+"|--")
               + c.green("ASSIGN_OPERATOR: ") + as->op
               + visit_nodes(as->rhs_exprs, indent+lead, true);
    }

    String visit(node::if_stmt const& is, String const& indent, char const* const lead) const noexcept
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

    String visit(node::case_stmt const& cs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(cs, indent)
                + visit_nodes_with_predicate(cs->when_stmts_list,
                        [this, indent, lead](auto const& cond_and_when_stmts, auto const l){
                            return visit(cond_and_when_stmts.first, indent+lead, "|  ")
                                + '\n' + visit(cond_and_when_stmts.second, indent+lead, l);
                        }, !cs->maybe_else_stmts)
                + (cs->maybe_else_stmts ? visit(*(cs->maybe_else_stmts), indent+lead, "   ") : "");
    }

    String visit(node::switch_stmt const& ss, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ss, indent)
                + '\n' + visit(ss->target_expr, indent+lead, ss->when_stmts_list.empty() && !ss->maybe_else_stmts ? "   " : "|  ")
                + visit_nodes_with_predicate(ss->when_stmts_list,
                        [this, &indent, lead](auto const& cond_and_when_stmts, auto const l)
                        {
                            // XXX: Workaround!
                            return boost::algorithm::join(
                                    cond_and_when_stmts.first | transformed(
                                            [this, &indent, lead](auto const& cond)
                                            {
                                                return visit(cond, indent+lead, "|  ");
                                            }
                                        ),
                                    "\n"
                                ) + '\n' + visit(cond_and_when_stmts.second, indent+lead, l);
                        }, !ss->maybe_else_stmts)
                + (ss->maybe_else_stmts ? visit(*(ss->maybe_else_stmts), indent+lead, "   ") : "");
    }

    String visit(node::return_stmt const& rs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(rs, indent) + visit_nodes(rs->ret_exprs, indent+lead, true);
    }

    String visit(node::for_stmt const& fs, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fs, indent)
                + visit_nodes(fs->iter_vars, indent+lead, false)
                + '\n' + visit(fs->range_expr, indent+lead, "|  ")
                + '\n' + visit(fs->body_stmts, indent+lead, "   ");
    }

    String visit(node::while_stmt const& ws, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(ws, indent)
                + '\n' + visit(ws->condition, indent+lead, "|  ")
                + '\n' + visit(ws->body_stmts, indent+lead, "   ");
    }

    String visit(node::postfix_if_stmt const& pis, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(pis, indent)
                + '\n' + visit_variant_node(pis->body, indent+lead, "|  ")
                + '\n' + visit(pis->condition, indent+lead, "   ");
    }

    String visit(node::variable_decl const& vd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(vd, indent)
            + (vd->maybe_type ? '\n' + visit(*(vd->maybe_type), indent+lead, "   ") : "");
    }

    String visit(node::initialize_stmt const& vds, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(vds, indent)
            + visit_nodes(vds->var_decls, indent+lead, !vds->maybe_rhs_exprs)
            + (vds->maybe_rhs_exprs ? visit_nodes(*(vds->maybe_rhs_exprs), indent+lead, true) : "");
    }

    String visit(node::statement_block const& sb, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(sb, indent) + visit_nodes(sb->value, indent+lead, true);
    }

    String visit(node::function_definition const& fd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(fd, indent)
            + visit_nodes(fd->params, indent+lead, false)
            + visit_optional_node(fd->return_type, indent+lead, "|  ")
            + '\n' + visit(fd->body, indent+lead, fd->ensure_body ? "|  " : "   ")
            + visit_optional_node(fd->ensure_body, indent+lead, "   ");
    }

    String visit(node::class_definition const& cd, String const& indent, char const* const lead) const noexcept
    {
        return prefix_of(cd, indent)
            + visit_nodes(cd->instance_vars, indent+lead, cd->member_funcs.empty())
            + visit_nodes(cd->member_funcs, indent+lead, true);
    }

    template<class... Args>
    String visit(boost::variant<Args...> const& v, String const& indent, char const* const lead) const noexcept
    {
        return visit_variant_node(v, indent, lead);
    }

    // For terminal nodes
    template<class T>
    String visit(std::shared_ptr<T> const& p, String const& indent, char const* const) const noexcept
    {
        static_assert(traits::is_node<T>::value, "ast_stringizer: visit a object which isn't AST node.");
        return prefix_of(p, indent);
    }
};

} // namespace detail

std::string stringize_ast(ast const& ast)
{
    return detail::ast_stringizer<std::string>{}.visit(ast.root, "");
}

} // namespace ast
} // namespace dachs
