#if !defined DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
#define      DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED

#include <vector>
#include <utility>
#include <memory>
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

template<class SourceNode, class Node = SourceNode, class... Args>
inline Node copy_node(SourceNode const& node, Args &&... args)
{
    auto copied = make<Node>(std::forward<Args>(args)...);
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
        return copy_node(p, *p);
    }

    auto copy(node::primary_literal const& pl) const
    {
        return copy_node(pl, pl->value);
    }

    auto copy(node::array_literal const& al) const
    {
        return copy_node(al, copy(al->element_exprs));
    }

    auto copy(node::tuple_literal const& tl) const
    {
        return copy_node(tl, copy(tl->element_exprs));
    }

    auto copy(node::string_literal const& sl) const
    {
        return copy_node(sl, sl->value);
    }

    auto copy(node::dict_literal const& dl) const
    {
        return copy_node(dl, copy(dl->value));
    }

    auto copy(node::parameter const& p) const
    {
        return copy_node(p, p->is_var, p->name, copy(p->param_type), p->is_receiver);
    }

    auto copy(node::func_invocation const& fc) const
    {
        return copy_node(fc, copy(fc->child), copy(fc->args), fc->is_ufcs);
    }

    auto copy(node::object_construct const& oc) const
    {
        return copy_node(oc, copy(oc->obj_type), copy(oc->args), oc->constructed_class_scope, oc->callee_ctor_scope);
    }

    auto copy(node::index_access const& ia) const
    {
        return copy_node(ia, copy(ia->child), copy(ia->index_expr));
    }

    auto copy(node::ufcs_invocation const& ui) const
    {
        return copy_node(ui, copy(ui->child), ui->member_name, ui->is_assign);
    }

    auto copy(node::unary_expr const& ue) const
    {
        return copy_node(ue, ue->op, copy(ue->expr));
    }

    auto copy(node::cast_expr const& ce) const
    {
        return copy_node(ce, copy(ce->child), copy(ce->cast_type));
    }

    auto copy(node::binary_expr const& be) const
    {
        return copy_node(be, copy(be->lhs), be->op, copy(be->rhs));
    }

    auto copy(node::block_expr const& be) const
    {
        return copy_node(be, copy(be->stmts), copy(be->last_expr));
    }

    auto copy(node::if_expr const& ie) const
    {
        return copy_node(
                ie,
                ie->kind,
                copy(ie->block_list),
                copy(ie->else_block)
            );
    }

    auto copy(node::switch_expr const& se) const
    {
        return copy_node(se, copy(se->target_expr), copy(se->when_blocks), copy(se->else_block));
    }

    auto copy(node::typed_expr const& te) const
    {
        return copy_node(te, copy(te->child_expr), copy(te->specified_type));
    }

    auto copy(node::primary_type const& tt) const
    {
        return copy_node(tt, tt->name, copy(tt->template_params));
    }

    auto copy(node::array_type const& at) const
    {
        return copy_node(at, copy(at->elem_type));
    }

    auto copy(node::dict_type const& dt) const
    {
        return copy_node(dt, copy(dt->key_type), copy(dt->value_type));
    }

    auto copy(node::pointer_type const& pt) const
    {
        return copy_node(pt, copy(pt->pointee_type));
    }

    auto copy(node::typeof_type const& tt) const
    {
        return copy_node(tt, copy(tt->expr));
    }

    auto copy(node::tuple_type const& tt) const
    {
        return copy_node(tt, copy(tt->arg_types));
    }

    auto copy(node::func_type const& ft) const
    {
        return copy_node(ft, copy(ft->arg_types), copy(ft->ret_type), ft->parens_missing);
    }

    auto copy(node::qualified_type const& qt) const
    {
        return copy_node(qt, qt->qualifier, copy(qt->type));
    }

    auto copy(node::variable_decl const& vd) const
    {
        return copy_node(vd, vd->is_var, vd->name, copy(vd->maybe_type), vd->accessibility);
    }

    auto copy(node::initialize_stmt const& is) const
    {
        return copy_node(is, copy(is->var_decls), copy(is->maybe_rhs_exprs));
    }

    auto copy(node::assignment_stmt const& as) const
    {
        return copy_node(as, copy(as->assignees), as->op, copy(as->rhs_exprs), as->rhs_tuple_expansion);
    }

    auto copy(node::statement_block const& sb) const
    {
        return copy_node(sb, copy(sb->value));
    }

    auto copy(node::if_stmt const& is) const
    {
        return copy_node(
                is,
                is->kind,
                copy(is->clauses),
                copy(is->maybe_else_clause)
            );
    }

    auto copy(node::return_stmt const& rs) const
    {
        return copy_node(rs, copy(rs->ret_exprs));
    }

    auto copy(node::switch_stmt const& ss) const
    {
        return copy_node(ss, copy(ss->target_expr), copy(ss->when_stmts_list), copy(ss->maybe_else_stmts));
    }

    auto copy(node::for_stmt const& fs) const
    {
        return copy_node(fs, copy(fs->iter_vars), copy(fs->range_expr), copy(fs->body_stmts));
    }

    auto copy(node::while_stmt const& ws) const
    {
        return copy_node(ws, copy(ws->condition), copy(ws->body_stmts));
    }

    auto copy(node::postfix_if_stmt const& pif) const
    {
        return copy_node(pif, copy(pif->body), pif->kind, copy(pif->condition));
    }

    auto copy(node::function_definition const& fd) const
    {
        return copy_node(
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
        return copy_node(
                    cd,
                    cd->name,
                    copy(cd->instance_vars),
                    copy(cd->member_funcs)
                );
    }

    auto copy(node::import const& i) const
    {
        return copy_node(
                    i,
                    i->path
                );
    }

    auto copy(node::lambda_expr const& le) const
    {
        return copy_node(le, copy(le->def));
    }

    auto copy(node::inu const& p) const
    {
        return copy_node(p, copy(p->functions), copy(p->global_constants), copy(p->classes), copy(p->imports));
    }

};

} // namespace detail

template<class Node>
auto copy_ast(std::shared_ptr<Node> const& node)
{
    return detail::copier{}.copy(node);
}

template<class... Nodes>
auto copy_ast(boost::variant<Nodes...> const& variant_node)
{
    return detail::copier{}.copy(variant_node);
}

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
