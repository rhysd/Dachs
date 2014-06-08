#if !defined DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED
#define      DACHS_AST_AST_COPIER_BASE_HPP_INCLUDED

#include <vector>
#include <utility>

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace ast {
namespace detail {

using helper::make;

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
        return make<std::shared_ptr<Terminal>>(*p);
    }

    auto copy(node::primary_literal const& pl) const
    {
        return make<node::primary_literal>(pl->value);
    }

    auto copy(node::array_literal const& al) const
    {
        return make<node::array_literal>(copy(al->element_exprs));
    }

    auto copy(node::tuple_literal const& tl) const
    {
        return make<node::tuple_literal>(copy(tl->element_exprs));
    }

    auto copy(node::dict_literal const& dl) const
    {
        return make<node::dict_literal>(copy(dl->value));
    }

    auto copy(node::parameter const& p) const
    {
        return make<node::parameter>(p->is_var, p->name, copy(p->param_type));
    }

    auto copy(node::func_invocation const& fc) const
    {
        return make<node::func_invocation>(copy(fc->child), copy(fc->args));
    }

    auto copy(node::object_construct const& oc) const
    {
        return make<node::object_construct>(copy(oc->obj_type), copy(oc->args));
    }

    auto copy(node::index_access const& ia) const
    {
        return make<node::index_access>(copy(ia->child), copy(ia->index_expr));
    }

    auto copy(node::member_access const& ma) const
    {
        return make<node::member_access>(copy(ma->child), ma->member_name);
    }

    auto copy(node::unary_expr const& ue) const
    {
        return make<node::unary_expr>(ue->op, copy(ue->expr));
    }

    auto copy(node::cast_expr const& ce) const
    {
        return make<node::cast_expr>(copy(ce->child), copy(ce->casted_type));
    }

    auto copy(node::binary_expr const& be) const
    {
        return make<node::binary_expr>(copy(be->lhs), be->op, copy(be->rhs));
    }

    auto copy(node::if_expr const& ie) const
    {
        return make<node::if_expr>(ie->kind, copy(ie->condition_expr), copy(ie->then_expr), copy(ie->else_expr));
    }

    auto copy(node::typed_expr const& te) const
    {
        return make<node::typed_expr>(copy(te->child_expr), copy(te->specified_type));
    }

    auto copy(node::primary_type const& tt) const
    {
        return make<node::primary_type>(tt->template_name, copy(tt->instantiated_templates));
    }

    auto copy(node::array_type const& at) const
    {
        return make<node::array_type>(copy(at->elem_type));
    }

    auto copy(node::dict_type const& dt) const
    {
        return make<node::dict_type>(copy(dt->key_type), copy(dt->value_type));
    }

    auto copy(node::tuple_type const& tt) const
    {
        return make<node::tuple_type>(copy(tt->arg_types));
    }

    auto copy(node::func_type const& ft) const
    {
        if (ft->ret_type) {
            return make<node::func_type>(copy(ft->arg_types), copy(*ft->ret_type));
        } else {
            return make<node::func_type>(copy(ft->arg_types));
        }
    }

    auto copy(node::qualified_type const& qt) const
    {
        return make<node::qualified_type>(qt->qualifier, copy(qt->type));
    }

    auto copy(node::variable_decl const& vd) const
    {
        return make<node::variable_decl>(vd->is_var, vd->name, copy(vd->maybe_type));
    }

    auto copy(node::initialize_stmt const& is) const
    {
        return make<node::initialize_stmt>(copy(is->var_decls), copy(is->maybe_rhs_exprs));
    }

    auto copy(node::assignment_stmt const& as) const
    {
        return make<node::assignment_stmt>(copy(as->assignees), as->assign_op, copy(as->rhs_exprs));
    }

    auto copy(node::statement_block const& sb) const
    {
        return make<node::statement_block>(copy(sb->value));
    }

    auto copy(node::if_stmt const& is) const
    {
        return make<node::if_stmt>(is->kind, copy(is->condition), copy(is->then_stmts), copy(is->elseif_stmts_list), copy(is->maybe_else_stmts));
    }

    auto copy(node::return_stmt const& rs) const
    {
        return make<node::return_stmt>(copy(rs->ret_exprs));
    }

    auto copy(node::case_stmt const& cs) const
    {
        return make<node::case_stmt>(copy(cs->when_stmts_list), copy(cs->maybe_else_stmts));
    }

    auto copy(node::switch_stmt const& ss) const
    {
        return make<node::switch_stmt>(copy(ss->target_expr), copy(ss->when_stmts_list), copy(ss->maybe_else_stmts));
    }

    auto copy(node::for_stmt const& fs) const
    {
        return make<node::for_stmt>(copy(fs->iter_vars), copy(fs->range_expr), copy(fs->body_stmts));
    }

    auto copy(node::while_stmt const& ws) const
    {
        return make<node::while_stmt>(copy(ws->condition), copy(ws->body_stmts));
    }

    auto copy(node::postfix_if_stmt const& pif) const
    {
        return make<node::postfix_if_stmt>(copy(pif->body), pif->kind, copy(pif->condition));
    }

    auto copy(node::function_definition const& fd) const
    {
        return make<node::function_definition>(
                    fd->kind,
                    fd->name,
                    copy(fd->params),
                    copy(fd->return_type),
                    copy(fd->body),
                    copy(fd->ensure_body)
                );
    }

    auto copy(node::program const& p) const
    {
        return make<node::program>(copy(p->inu));
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
