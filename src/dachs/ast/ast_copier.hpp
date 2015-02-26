#if !defined DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
#define      DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED

#include <vector>
#include <utility>
#include <cstddef>

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace ast {
namespace detail {

using std::size_t;

template<class T>
auto location_of(T const& node) noexcept
{
    return node.source_location();
}

template<class T>
auto location_of(std::shared_ptr<T> const& node) noexcept
{
    return node->source_location();
}

template<class Node, class SourceNode, class... Args>
inline Node copy_node(SourceNode const& node, Args &&... args)
{
    auto copied = helper::make<Node>(std::forward<Args>(args)...);
    copied->set_source_location(location_of(node));
    return copied;
}

class copier {
public:
    template<class... Nodes>
    auto copy(boost::variant<Nodes...> const& v) const
    {
        return helper::variant::apply_lambda(
                [this](auto const& n)
                    ->boost::variant<Nodes...>
                {
                    return {copy(n)};
                }, v
            );
    }

    template<class T>
    boost::optional<T> copy(boost::optional<T> const& o) const
    {
        if(o) {
            return copy(*o);
        } else {
            return boost::none;
        }
    }

    template<class T>
    auto copy(std::vector<T> const& v) const
    {
        std::vector<T> ret;
        ret.reserve(v.size());
        for (auto const& n : v) {
            ret.push_back(copy(n));
        }
        return ret;
    }

    template<class T, class U>
    auto copy(std::pair<T, U> const& p) const
    {
        return std::make_pair(copy(p.first), copy(p.second));
    }

    template<class Terminal>
    auto copy(std::shared_ptr<Terminal> const& p) const
    {
        return copy_node<std::shared_ptr<Terminal>>(p, *p);
    }

    auto copy(node::primary_literal const& pl) const
    {
        return copy_node<node::primary_literal>(pl, pl->value);
    }

    auto copy(node::array_literal const& al) const
    {
        return copy_node<node::array_literal>(al, copy(al->element_exprs));
    }

    auto copy(node::tuple_literal const& tl) const
    {
        return copy_node<node::tuple_literal>(tl, copy(tl->element_exprs));
    }

    auto copy(node::dict_literal const& dl) const
    {
        return copy_node<node::dict_literal>(dl, copy(dl->value));
    }

    auto copy(node::parameter const& p) const
    {
        return copy_node<node::parameter>(p, p->is_var, p->name, copy(p->param_type), p->is_receiver);
    }

    auto copy(node::func_invocation const& fc) const
    {
        return copy_node<node::func_invocation>(fc, copy(fc->child), copy(fc->args), fc->is_ufcs);
    }

    auto copy(node::object_construct const& oc) const
    {
        return copy_node<node::object_construct>(oc, copy(oc->obj_type), copy(oc->args), oc->constructed_class_scope, oc->callee_ctor_scope);
    }

    auto copy(node::index_access const& ia) const
    {
        return copy_node<node::index_access>(ia, copy(ia->child), copy(ia->index_expr));
    }

    auto copy(node::ufcs_invocation const& ui) const
    {
        return copy_node<node::ufcs_invocation>(ui, copy(ui->child), ui->member_name);
    }

    auto copy(node::unary_expr const& ue) const
    {
        return copy_node<node::unary_expr>(ue, ue->op, copy(ue->expr));
    }

    auto copy(node::cast_expr const& ce) const
    {
        return copy_node<node::cast_expr>(ce, copy(ce->child), copy(ce->casted_type));
    }

    auto copy(node::binary_expr const& be) const
    {
        return copy_node<node::binary_expr>(be, copy(be->lhs), be->op, copy(be->rhs));
    }

    auto copy(node::if_expr const& ie) const
    {
        return copy_node<node::if_expr>(ie, ie->kind, copy(ie->condition_expr), copy(ie->then_expr), copy(ie->else_expr));
    }

    auto copy(node::typed_expr const& te) const
    {
        return copy_node<node::typed_expr>(te, copy(te->child_expr), copy(te->specified_type));
    }

    auto copy(node::primary_type const& tt) const
    {
        return copy_node<node::primary_type>(tt, tt->template_name, copy(tt->holders));
    }

    auto copy(node::array_type const& at) const
    {
        return copy_node<node::array_type>(at, copy(at->elem_type));
    }

    auto copy(node::dict_type const& dt) const
    {
        return copy_node<node::dict_type>(dt, copy(dt->key_type), copy(dt->value_type));
    }

    auto copy(node::tuple_type const& tt) const
    {
        return copy_node<node::tuple_type>(tt, copy(tt->arg_types));
    }

    auto copy(node::func_type const& ft) const
    {
        if (ft->ret_type) {
            return copy_node<node::func_type>(ft, copy(ft->arg_types), copy(*ft->ret_type));
        } else {
            return copy_node<node::func_type>(ft, copy(ft->arg_types));
        }
    }

    auto copy(node::qualified_type const& qt) const
    {
        return copy_node<node::qualified_type>(qt, qt->qualifier, copy(qt->type));
    }

    auto copy(node::variable_decl const& vd) const
    {
        return copy_node<node::variable_decl>(vd, vd->is_var, vd->name, copy(vd->maybe_type), vd->accessibility);
    }

    auto copy(node::initialize_stmt const& is) const
    {
        return copy_node<node::initialize_stmt>(is, copy(is->var_decls), copy(is->maybe_rhs_exprs));
    }

    auto copy(node::assignment_stmt const& as) const
    {
        return copy_node<node::assignment_stmt>(as, copy(as->assignees), as->op, copy(as->rhs_exprs));
    }

    auto copy(node::statement_block const& sb) const
    {
        return copy_node<node::statement_block>(sb, copy(sb->value));
    }

    auto copy(node::if_stmt const& is) const
    {
        return copy_node<node::if_stmt>(is, is->kind, copy(is->condition), copy(is->then_stmts), copy(is->elseif_stmts_list), copy(is->maybe_else_stmts));
    }

    auto copy(node::return_stmt const& rs) const
    {
        return copy_node<node::return_stmt>(rs, copy(rs->ret_exprs));
    }

    auto copy(node::case_stmt const& cs) const
    {
        return copy_node<node::case_stmt>(cs, copy(cs->when_stmts_list), copy(cs->maybe_else_stmts));
    }

    auto copy(node::switch_stmt const& ss) const
    {
        return copy_node<node::switch_stmt>(ss, copy(ss->target_expr), copy(ss->when_stmts_list), copy(ss->maybe_else_stmts));
    }

    auto copy(node::for_stmt const& fs) const
    {
        return copy_node<node::for_stmt>(fs, copy(fs->iter_vars), copy(fs->range_expr), copy(fs->body_stmts));
    }

    auto copy(node::while_stmt const& ws) const
    {
        return copy_node<node::while_stmt>(ws, copy(ws->condition), copy(ws->body_stmts));
    }

    auto copy(node::postfix_if_stmt const& pif) const
    {
        return copy_node<node::postfix_if_stmt>(pif, copy(pif->body), pif->kind, copy(pif->condition));
    }

    auto copy(node::let_stmt const& ls) const
    {
        return copy_node<node::let_stmt>(ls, copy(ls->inits), copy(ls->child_stmt));
    }

    auto copy(node::function_definition const& fd) const
    {
        return copy_node<node::function_definition>(
                    fd,
                    fd->kind,
                    fd->name,
                    copy(fd->params),
                    copy(fd->return_type),
                    copy(fd->body),
                    copy(fd->ensure_body),
                    fd->accessibility
                );
    }

    auto copy(node::class_definition const& cd) const
    {
        return copy_node<node::class_definition>(
                    cd,
                    cd->name,
                    copy(cd->instance_vars),
                    copy(cd->member_funcs)
                );
    }

    auto copy(node::lambda_expr const& le) const
    {
        return copy_node<node::lambda_expr>(le, copy(le->def));
    }

    auto copy(node::inu const& p) const
    {
        return copy_node<node::inu>(p, copy(p->functions), copy(p->global_constants), copy(p->classes));
    }

};

} // namespace detail

template<class ASTNode>
ASTNode copy_ast(ASTNode const& node)
{
    static_assert(helper::is_shared_ptr<ASTNode>::value, "try to copy somthing which isn't AST node");
    return detail::copier{}.copy(node);
}

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
