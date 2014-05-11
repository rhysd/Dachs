#if !defined DACHS_AST_WALKER_BASE_HPP_INCLUDED
#define      DACHS_AST_WALKER_BASE_HPP_INCLUDED

#include <vector>
#include <utility>

#include <boost/optional.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/ast.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace ast {

namespace detail {

template<class Walker>
struct ast_walker_variant_visitor
        : public boost::static_visitor<void> {
    Walker &walker;

    explicit ast_walker_variant_visitor(Walker &w) noexcept
        : walker(w)
    {}

    template<class T>
    void operator()(T &value)
    {
        walker.walk(value);
    }
};

} // namespace detail

// Note:
// A walker can have some callbacks.
// I can implement them when needed.
//   - initialize() : will be called at the beginning of visiting AST node
//   - finalize()   : will be called at the end of visiting AST node

template<class Visitor>
class walker {

    detail::ast_walker_variant_visitor<walker> variant_visitor{*this};
    Visitor &visitor;

    template<class T>
    void walk_optional(boost::optional<T> &o)
    {
        if(o) {
            walk(*o);
        }
    }

    template<class T>
    void walk_vector(std::vector<T> &v)
    {
        for (auto &n : v) {
            walk(n);
        }
    }

    template<class T, class U>
    void walk_pair(std::pair<T, U> &p)
    {
        walk(p.first);
        walk(p.second);
    }

    template<class T>
    void walk_mult_binary_expr(T &e)
    {
        walk(e->lhs);
        for (auto &p : e->rhss) {
            walk(p.second);
        }
    }

    template<class T>
    void walk_binary_expr(T &e)
    {
        walk(e->lhs);
        walk_vector(e->rhss);
    }

public:

    explicit walker(Visitor &v) noexcept
        : visitor(v)
    {}

    void walk(node::array_literal &al)
    {
        visitor.visit(al, [&]{
            walk_vector(al->element_exprs);
        });
    }

    void walk(node::tuple_literal &tl)
    {
        visitor.visit(tl, [&]{
            walk_vector(tl->element_exprs);
        });
    }

    void walk(node::dict_literal &dl)
    {
        visitor.visit(dl, [&]{
            for (auto &p : dl->value) {
                walk_pair(p);
            }
        });
    }

    void walk(node::parameter &p)
    {
        visitor.visit(p, [&]{
            walk_optional(p->param_type);
        });
    }

    void walk(node::func_invocation &fc)
    {
        visitor.visit(fc, [&]{
            walk(fc->child);
            walk_vector(fc->args);
        });
    }

    void walk(node::object_construct &oc)
    {
        visitor.visit(oc, [&]{
            walk(oc->obj_type);
            walk_vector(oc->args);
        });
    }

    void walk(node::index_access &ia)
    {
        visitor.visit(ia, [&]{
            walk(ia->child);
            walk(ia->index_expr);
        });
    }

    void walk(node::member_access &ma)
    {
        visitor.visit(ma, [&]{
            walk(ma->child);
        });
    }

    void walk(node::unary_expr &ue)
    {
        visitor.visit(ue, [&]{
            walk(ue->expr);
        });
    }

    void walk(node::primary_type &tt)
    {
        visitor.visit(tt, [&]{
            walk_vector(tt->instantiated_templates);
        });
    }

    void walk(node::array_type &at)
    {
        visitor.visit(at, [&]{
            walk(at->elem_type);
        });
    }

    void walk(node::dict_type &dt)
    {
        visitor.visit(dt, [&]{
            walk(dt->key_type);
            walk(dt->value_type);
        });
    }

    void walk(node::tuple_type &tt)
    {
        visitor.visit(tt, [&]{
            walk_vector(tt->arg_types);
        });
    }

    void walk(node::func_type &ft)
    {
        visitor.visit(ft, [&]{
            walk_vector(ft->arg_types);
            walk_optional(ft->ret_type);
        });
    }

    void walk(node::qualified_type &qt)
    {
        visitor.visit(qt, [&]{
            walk(qt->type);
        });
    }

    void walk(node::cast_expr &ce)
    {
        visitor.visit(ce, [&]{
            walk(ce->child);
            walk(ce->casted_type);
        });
    }

    void walk(node::binary_expr &be)
    {
        visitor.visit(be, [&]{
            walk(be->rhs);
            walk(be->lhs);
        });
    }

    void walk(node::if_expr &ie)
    {
        visitor.visit(ie, [&]{
            walk(ie->condition_expr);
            walk(ie->then_expr);
            walk(ie->else_expr);
        });
    }

    void walk(node::typed_expr &te)
    {
        visitor.visit(te, [&]{
            walk(te->child_expr);
            walk(te->specified_type);
        });
    }

    void walk(node::variable_decl &vd)
    {
        visitor.visit(vd, [&]{
            walk_optional(vd->maybe_type);
        });
    }

    void walk(node::initialize_stmt &is)
    {
        visitor.visit(is, [&]{
            walk_vector(is->var_decls);
            if (is->maybe_rhs_exprs) {
                walk_vector(*(is->maybe_rhs_exprs));
            }
        });
    }

    void walk(node::assignment_stmt &as)
    {
        visitor.visit(as, [&]{
            walk_vector(as->assignees);
            walk_vector(as->rhs_exprs);
        });
    }

    void walk(node::if_stmt &is)
    {
        visitor.visit(is, [&]{
            walk(is->condition);
            walk(is->then_stmts);
            for (auto &p : is->elseif_stmts_list) {
                walk_pair(p);
            }
            walk_optional(is->maybe_else_stmts);
        });
    }

    void walk(node::return_stmt &rs)
    {
        visitor.visit(rs, [&]{
            walk_vector(rs->ret_exprs);
        });
    }

    void walk(node::case_stmt &cs)
    {
        visitor.visit(cs, [&]{
            for (auto &p : cs->when_stmts_list) {
                walk_pair(p);
            }
            walk_optional(cs->maybe_else_stmts);
        });
    }

    void walk(node::switch_stmt &ss)
    {
        visitor.visit(ss, [&]{
            walk(ss->target_expr);
            for (auto &p : ss->when_stmts_list) {
                walk_vector(p.first);
                walk(p.second);
            }
            walk_optional(ss->maybe_else_stmts);
        });
    }

    void walk(node::for_stmt &fs)
    {
        visitor.visit(fs, [&]{
            walk_vector(fs->iter_vars);
            walk(fs->range_expr);
            walk(fs->body_stmts);
        });
    }

    void walk(node::while_stmt &ws)
    {
        visitor.visit(ws, [&]{
            walk(ws->condition);
            walk(ws->body_stmts);
        });
    }

    void walk(node::postfix_if_stmt &pif)
    {
        visitor.visit(pif, [&]{
            boost::apply_visitor(variant_visitor, pif->body);
            walk(pif->condition);
        });
    }

    void walk(node::statement_block &sb)
    {
        visitor.visit(sb, [&]{
            walk_vector(sb->value);
        });
    }

    void walk(node::function_definition &fd)
    {
        visitor.visit(fd, [&]{
            for (auto &p : fd->params) {
                walk(p);
            }
            walk_optional(fd->return_type);
            walk(fd->body);
            walk_optional(fd->ensure_body);
        });
    }

    void walk(node::constant_decl &cd)
    {
        visitor.visit(cd, [&]{
            walk_optional(cd->maybe_type);
        });
    }

    void walk(node::constant_definition &cd)
    {
        visitor.visit(cd, [&]{
            walk_vector(cd->const_decls);
            walk_vector(cd->initializers);
        });
    }

    void walk(node::program &p)
    {
        visitor.visit(p, [&]{
            walk_vector(p->inu);
        });
    }

    template<class... Nodes>
    void walk(boost::variant<Nodes...> &v)
    {
        boost::apply_visitor(variant_visitor, v);
    }

    template<class Terminal>
    void walk(std::shared_ptr<Terminal> &p)
    {
        visitor.visit(p, [&]{});
    }

};

template<class Visitor>
inline auto make_walker(Visitor &v)
{
    return walker<Visitor>{v};
}

template<class ASTNode, class Visitor>
inline void walk_topdown(ASTNode &n, Visitor &v)
{
    static_assert(helper::is_shared_ptr<ASTNode>::value, "walk on somthing which isn't AST node");
    make_walker(v).walk(n);
}

template<class ASTNode, class Visitor>
inline void walk_topdown(ASTNode &n, Visitor && v)
{
    static_assert(helper::is_shared_ptr<ASTNode>::value, "walk on somthing which isn't AST node");
    make_walker(v).walk(n);
}

} // namespace ast

} // namespace dachs

#endif    // DACHS_AST_WALKER_BASE_HPP_INCLUDED
