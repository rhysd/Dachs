#if !defined DACHS_AST_AST_WALKER_BASE_HPP_INCLUDED
#define      DACHS_AST_AST_WALKER_BASE_HPP_INCLUDED

#include <vector>
#include <utility>

#include <boost/optional.hpp>
#include <boost/variant/variant.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace ast {

// Note:
// A walker can have some callbacks.
// I can implement them when needed.
//   - initialize() : will be called at the beginning of visiting AST node
//   - finalize()   : will be called at the end of visiting AST node

template<class Visitor>
class walker {

    Visitor &visitor;

    void walk_all()
    {}

    template<class Head, class... Tail>
    void walk_all(Head &h, Tail &... t)
    {
        walk(h);
        walk_all(t...);
    }

    template<class... Args>
    auto walker_for(Args &... children)
    {
        // Note:
        // It is no need to deal with below conditional branch on compile time
        // because I can expect optimization.
        // c.f. https://gist.github.com/rhysd/09962fbf6d86a94c7b7e
        return [&, this](auto &... args){
            if (sizeof...(args) == 0) {
                walk_all(children...);
            } else {
                walk_all(args...);
            }
        };
    }

public:

    explicit walker(Visitor &v) noexcept
        : visitor(v)
    {}

    void walk(node::array_literal &al)
    {
        visitor.visit(al, walker_for(
            al->element_exprs
        ));
    }

    void walk(node::tuple_literal &tl)
    {
        visitor.visit(tl, walker_for(
            tl->element_exprs
        ));
    }

    void walk(node::dict_literal &dl)
    {
        visitor.visit(dl, walker_for(
            dl->value
        ));
    }

    void walk(node::parameter &p)
    {
        visitor.visit(p, walker_for(
            p->param_type
        ));
    }

    void walk(node::func_invocation &fc)
    {
        visitor.visit(fc, walker_for(
            fc->child,
            fc->args
        ));
    }

    void walk(node::object_construct &oc)
    {
        visitor.visit(oc, walker_for(
            oc->obj_type,
            oc->args
        ));
    }

    void walk(node::index_access &ia)
    {
        visitor.visit(ia, walker_for(
            ia->child,
            ia->index_expr
        ));
    }

    void walk(node::ufcs_invocation &ui)
    {
        visitor.visit(ui, walker_for(
            ui->child
        ));
    }

    void walk(node::unary_expr &ue)
    {
        visitor.visit(ue, walker_for(
            ue->expr
        ));
    }

    void walk(node::primary_type &tt)
    {
        visitor.visit(tt, walker_for(
            tt->template_params
        ));
    }

    void walk(node::array_type &at)
    {
        visitor.visit(at, walker_for(
            at->elem_type
        ));
    }

    void walk(node::dict_type &dt)
    {
        visitor.visit(dt, walker_for(
            dt->key_type,
            dt->value_type
        ));
    }

    void walk(node::pointer_type &pt)
    {
        visitor.visit(pt, walker_for(
            pt->pointee_type
        ));
    }

    void walk(node::typeof_type &tt)
    {
        visitor.visit(tt, walker_for(
            tt->expr
        ));
    }

    void walk(node::tuple_type &tt)
    {
        visitor.visit(tt, walker_for(
            tt->arg_types
        ));
    }

    void walk(node::func_type &ft)
    {
        visitor.visit(ft, walker_for(
            ft->arg_types,
            ft->ret_type
        ));
    }

    void walk(node::qualified_type &qt)
    {
        visitor.visit(qt, walker_for(
            qt->type
        ));
    }

    void walk(node::cast_expr &ce)
    {
        visitor.visit(ce, walker_for(
            ce->child,
            ce->cast_type
        ));
    }

    void walk(node::binary_expr &be)
    {
        visitor.visit(be, walker_for(
            be->rhs,
            be->lhs
        ));
    }

    void walk(node::block_expr &be)
    {
        visitor.visit(be, walker_for(
            be->stmts,
            be->last_expr
        ));
    }

    void walk(node::if_expr &ie)
    {
        visitor.visit(ie, walker_for(
            ie->block_list,
            ie->else_block
        ));
    }

    void walk(node::typed_expr &te)
    {
        visitor.visit(te, walker_for(
            te->child_expr,
            te->specified_type
        ));
    }

    void walk(node::variable_decl &vd)
    {
        visitor.visit(vd, walker_for(
            vd->maybe_type
        ));
    }

    void walk(node::initialize_stmt &is)
    {
        visitor.visit(is, walker_for(
            is->maybe_rhs_exprs,
            is->var_decls
        ));
    }

    void walk(node::assignment_stmt &as)
    {
        visitor.visit(as, walker_for(
            as->rhs_exprs,
            as->assignees
        ));
    }

    void walk(node::if_stmt &is)
    {
        visitor.visit(is, walker_for(
            is->clauses,
            is->maybe_else_clause
        ));
    }

    void walk(node::return_stmt &rs)
    {
        visitor.visit(rs, walker_for(
            rs->ret_exprs
        ));
    }

    void walk(node::switch_stmt &ss)
    {
        visitor.visit(ss, walker_for(
            ss->target_expr,
            ss->when_stmts_list,
            ss->maybe_else_stmts
        ));
    }

    void walk(node::for_stmt &fs)
    {
        visitor.visit(fs, walker_for(
            fs->iter_vars,
            fs->range_expr,
            fs->body_stmts
        ));
    }

    void walk(node::while_stmt &ws)
    {
        visitor.visit(ws, walker_for(
            ws->condition,
            ws->body_stmts
        ));
    }

    void walk(node::postfix_if_stmt &pif)
    {
        visitor.visit(pif, walker_for(
            pif->body,
            pif->condition
        ));
    }

    void walk(node::statement_block &sb)
    {
        visitor.visit(sb, walker_for(
            sb->value
        ));
    }

    void walk(node::function_definition &fd)
    {
        visitor.visit(fd, walker_for(
            fd->params,
            fd->return_type,
            fd->body,
            fd->ensure_body
        ));
    }

    void walk(node::class_definition &cd)
    {
        visitor.visit(cd, walker_for(
            cd->instance_vars,
            cd->member_funcs
        ));
    }

    void walk(node::inu &p)
    {
        visitor.visit(p, walker_for(
            p->functions,
            p->global_constants,
            p->classes,
            p->imports
        ));
    }

    void walk(node::any_expr &a)
    {
        visitor.visit(a, [&,this]{
                helper::variant::apply_lambda([this](auto &n) { walk(n); }, a);
            });
    }

    template<class... Nodes>
    void walk(boost::variant<Nodes...> &v)
    {
        helper::variant::apply_lambda([this](auto &n) { walk(n); }, v);
    }

    template<class T>
    void walk(boost::optional<T> &o)
    {
        if(o) {
            walk(*o);
        }
    }

    template<class T>
    void walk(std::vector<T> &v)
    {
        for (auto &n : v) {
            walk(n);
        }
    }

    template<class T, class U>
    void walk(std::pair<T, U> &p)
    {
        walk(p.first);
        walk(p.second);
    }

    template<class Terminal>
    void walk(std::shared_ptr<Terminal> &p)
    {
        visitor.visit(p, [&]{});
    }

};

template<class Visitor>
inline auto make_walker(Visitor && v)
{
    return walker<Visitor>{v};
}

template<class ASTNode, class Visitor>
inline void walk_topdown(ASTNode &n, Visitor && v)
{
    make_walker(v).walk(n);
}

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_WALKER_BASE_HPP_INCLUDED
