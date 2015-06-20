#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <cstddef>
#include <boost/lexical_cast.hpp>
#include <boost/range/adaptor/sliced.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace ast {
namespace detail {

using std::size_t;
using boost::adaptors::sliced;

template<class String>
class ast_stringizer {
    using cstr = char const* const;

    helper::colorizer c;

    template<class T>
    String prefix_of(std::shared_ptr<T> const& p, String const& indent) const
    {
        return c.yellow(indent+"|\n"+indent+"|--", false)
            + c.green(p->to_string(), false)
            + c.gray(" (" + p->location.to_string(false) + ')')
            + '\n';
    }

public:

    String stringize(node::inu const& p, String const& indent) const
    {
        return prefix_of(p, indent)
            + stringize(p->functions, indent + "   ", p->global_constants.empty() && p->classes.empty() && p->imports.empty() ? "   " : "|  ")
            + stringize(p->global_constants, indent + "   ", p->classes.empty() && p->imports.empty() ? "   " : "|  ")
            + stringize(p->classes, indent + "   ", p->imports.empty() ? "   " : "|  ")
            + stringize(p->imports, indent + "   ", "   ");
    }

    String stringize(node::array_literal const& al, String const& indent, cstr lead) const
    {
        return prefix_of(al, indent) + stringize(al->element_exprs, indent+lead, "   ");
    }

    String stringize(node::tuple_literal const& tl, String const& indent, cstr lead) const
    {
        return prefix_of(tl, indent) + stringize(tl->element_exprs, indent+lead, "   ");
    }

    String stringize(node::lambda_expr const& le, String const& indent, cstr lead) const
    {
        return prefix_of(le, indent) + stringize(le->def, indent+lead, "   ");
    }

    String stringize(node::dict_literal const& ml, String const& indent, cstr lead) const
    {
        return prefix_of(ml, indent) + stringize(ml->value, indent+lead, "   ");
    }

    String stringize(node::parameter const& p, String const& indent, cstr lead) const
    {
        return prefix_of(p, indent)
                + stringize(p->param_type, indent+lead, "   ");
    }

    String stringize(node::object_construct const& oc, String const& indent, cstr lead) const
    {
        return prefix_of(oc, indent)
            + stringize(oc->obj_type, indent+lead, (oc->args.empty() ? "   " : "|  "))
            + stringize(oc->args, indent+lead, "   ");
    }

    String stringize(node::index_access const& ia, String const& indent, cstr lead) const
    {
        return prefix_of(ia, indent)
            + stringize(ia->child, indent+lead, "|  ")
            + stringize(ia->index_expr, indent+lead, "   ");
    }

    String stringize(node::ufcs_invocation const& ui, String const& indent, cstr lead) const
    {
        return prefix_of(ui, indent) + stringize(ui->child, indent+lead, "   ");
    }

    String stringize(node::func_invocation const& fc, String const& indent, cstr lead) const
    {
        return prefix_of(fc, indent)
            + stringize(fc->child, indent+lead, fc->args.empty() ? "   " : "|  ")
            + stringize(fc->args, indent+lead, "   ");
    }

    String stringize(node::unary_expr const& ue, String const& indent, cstr lead) const
    {
        return prefix_of(ue, indent) + stringize(ue->expr, indent+lead, "   ");
    }

    String stringize(node::primary_type const& tt, String const& indent, cstr lead) const
    {
        return prefix_of(tt, indent) + stringize(tt->template_params, indent+lead, "   ");
    }

    String stringize(node::array_type const& at, String const& indent, cstr lead) const
    {
        return prefix_of(at, indent) + stringize(at->elem_type, indent+lead, "   ");
    }

    String stringize(node::dict_type const& mt, String const& indent, cstr lead) const
    {
        return prefix_of(mt, indent)
            + stringize(mt->key_type, indent+lead, "|  ")
            + stringize(mt->value_type, indent+lead, "   ");
    }

    String stringize(node::pointer_type const& pt, String const& indent, cstr lead) const
    {
        return prefix_of(pt, indent)
            + stringize(pt->pointee_type, indent+lead, "   ");
    }

    String stringize(node::typeof_type const& tt, String const& indent, cstr lead) const
    {
        return prefix_of(tt, indent)
            + stringize(tt->expr, indent+lead, "   ");
    }

    String stringize(node::tuple_type const& tt, String const& indent, cstr lead) const
    {
        return prefix_of(tt, indent) + stringize(tt->arg_types, indent+lead, "   ");
    }

    String stringize(node::func_type const& ft, String const& indent, cstr lead) const
    {
        return prefix_of(ft, indent)
            + stringize(ft->arg_types, indent+lead, "|  ")
            + stringize(ft->ret_type, indent+lead, "   ");
    }

    String stringize(node::qualified_type const& qt, String const& indent, cstr lead) const
    {
        return prefix_of(qt, indent) + stringize(qt->type, indent+lead, "   ");
    }

    String stringize(node::cast_expr const& ce, String const& indent, cstr lead) const
    {
        return prefix_of(ce, indent)
                + stringize(ce->child, indent+lead, "|  ")
                + stringize(ce->cast_type, indent+lead, "   ");
    }

    String stringize(node::binary_expr const& be, String const& indent, cstr lead) const
    {
        return prefix_of(be, indent)
                + stringize(be->lhs, indent+lead, "|  ")
                + stringize(be->rhs, indent+lead, "   ");
    }

    String stringize(node::block_expr const& be, String const& indent, cstr lead) const
    {
        return prefix_of(be, indent)
            + stringize(be->stmts, indent+lead, "|  ")
            + stringize(be->last_expr, indent+lead, "   ");
    }

    String stringize(node::if_expr const& ie, String const& indent, cstr lead) const
    {
        return prefix_of(ie, indent)
                + stringize(ie->block_list, indent+lead, "|  ")
                + stringize(ie->else_block, indent+lead, "   ");
    }

    String stringize(node::switch_expr const& se, String const& indent, cstr lead) const
    {
        return prefix_of(se, indent)
                + stringize(se->target_expr, indent+lead, "|  ")
                + stringize(se->when_blocks, indent+lead, "|  ")
                + stringize(se->else_block, indent+lead, "   ");
    }

    String stringize(node::typed_expr const& te, String const& indent, cstr lead) const
    {
        return prefix_of(te, indent)
                + stringize(te->child_expr, indent+lead, "|  ")
                + stringize(te->specified_type, indent+lead, "   ");
    }

    String stringize(node::assignment_stmt const& as, String const& indent, cstr lead) const
    {
        return prefix_of(as, indent)
               + stringize(as->assignees, indent+lead, "|  ")
               + stringize(as->rhs_exprs, indent+lead, "   ");
    }

    String stringize(node::if_stmt const& is, String const& indent, cstr lead) const
    {
        return prefix_of(is, indent)
                + stringize(is->clauses, indent+lead, !is->maybe_else_clause ? "   " : "|  ")
                + stringize(is->maybe_else_clause, indent+lead, "   ");
    }

    String stringize(node::switch_stmt const& ss, String const& indent, cstr lead) const
    {
        return prefix_of(ss, indent)
                + stringize(ss->target_expr, indent+lead, ss->when_stmts_list.empty() && !ss->maybe_else_stmts ? "   " : "|  ")
                + stringize(ss->when_stmts_list, indent+lead, !ss->maybe_else_stmts ? "   " : "|  ")
                + stringize(ss->maybe_else_stmts, indent+lead, "   ");
    }

    String stringize(node::return_stmt const& rs, String const& indent, cstr lead) const
    {
        return prefix_of(rs, indent) + stringize(rs->ret_exprs, indent+lead, "   ");
    }

    String stringize(node::for_stmt const& fs, String const& indent, cstr lead) const
    {
        return prefix_of(fs, indent)
                + stringize(fs->iter_vars, indent+lead, "|  ")
                + stringize(fs->range_expr, indent+lead, "|  ")
                + stringize(fs->body_stmts, indent+lead, "   ");
    }

    String stringize(node::while_stmt const& ws, String const& indent, cstr lead) const
    {
        return prefix_of(ws, indent)
                + stringize(ws->condition, indent+lead, "|  ")
                + stringize(ws->body_stmts, indent+lead, "   ");
    }

    String stringize(node::postfix_if_stmt const& pis, String const& indent, cstr lead) const
    {
        return prefix_of(pis, indent)
                + stringize(pis->body, indent+lead, "|  ")
                + stringize(pis->condition, indent+lead, "   ");
    }

    String stringize(node::variable_decl const& vd, String const& indent, cstr lead) const
    {
        return prefix_of(vd, indent)
            + stringize(vd->maybe_type, indent+lead, "   ");
    }

    String stringize(node::initialize_stmt const& vds, String const& indent, cstr lead) const
    {
        return prefix_of(vds, indent)
            + stringize(vds->var_decls, indent+lead, !vds->maybe_rhs_exprs ? "   " : "|  ")
            + stringize(vds->maybe_rhs_exprs, indent+lead, "   ");
    }

    String stringize(node::statement_block const& sb, String const& indent, cstr lead) const
    {
        return prefix_of(sb, indent) + stringize(sb->value, indent+lead, "   ");
    }

    String stringize(node::function_definition const& fd, String const& indent, cstr lead) const
    {
        return prefix_of(fd, indent)
            + stringize(fd->params, indent+lead, "|  ")
            + stringize(fd->return_type, indent+lead, "|  ")
            + stringize(fd->body, indent+lead, fd->ensure_body ? "|  " : "   ")
            + stringize(fd->ensure_body, indent+lead, "   ");
    }

    String stringize(node::class_definition const& cd, String const& indent, cstr lead) const
    {
        return prefix_of(cd, indent)
            + stringize(cd->instance_vars, indent+lead, cd->member_funcs.empty() ? "   " : "|  ")
            + stringize(cd->member_funcs, indent+lead, "   ");
    }

    template<class... Args>
    String stringize(boost::variant<Args...> const& v, String const& indent, cstr lead) const
    {
        return helper::variant::apply_lambda(
                [this, &indent, lead](auto const& n){ return stringize(n, indent, lead); },
                v
            );
    }

    template<class Node>
    String stringize(std::vector<Node> const& nodes, String const& indent, cstr lead) const
    {
        if (nodes.empty()) {
            return "";
        }

        String s = "";
        for (auto const& n : nodes | sliced(0, nodes.size()-1)) {
            s += stringize(n, indent, "|  ");
        }

        return s + stringize(nodes.back(), indent, lead);
    }

    template<class T>
    String stringize(boost::optional<T> const& o, String const& indent, cstr lead) const
    {
        return o ? stringize(*o, indent, lead) : "";
    }

    template<class T, class U>
    String stringize(std::pair<T, U> const& node_pair, String const& indent, cstr lead) const
    {
        return stringize(node_pair.first, indent, "|  ")
            + stringize(node_pair.second, indent, lead);
    }

    // For terminal nodes
    template<class T>
    String stringize(std::shared_ptr<T> const& p, String const& indent, cstr) const
    {
        static_assert(traits::is_node<T>::value, "ast_stringizer: Stringize a object which isn't AST node.");
        return prefix_of(p, indent);
    }
};

} // namespace detail

std::string stringize_ast(ast const& ast)
{
    return detail::ast_stringizer<std::string>{}.stringize(ast.root, "");
}

} // namespace ast
} // namespace dachs
