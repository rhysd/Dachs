#if !defined DACHS_HELPER_AST_WALKER_BASE_HPP_INCLUDED
#define      DACHS_HELPER_AST_WALKER_BASE_HPP_INCLUDED

#include <vector>

#include <boost/optional.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/ast.hpp"

namespace dachs {
namespace helper {

namespace detail {


template<class Walker>
struct walker_variant_visit_helper
        : public boost::static_visitor<void> {
    Walker &walker;

    explicit walker_variant_visit_helper(Walker &w)
        : walker(w)
    {}

    template<class T>
    void operator()(T const& value)
    {
        walker.visit(value);
    }
};

} // namespace detail

class ast_walker_base {

    detail::walker_variant_visit_helper<ast_walker_base> variant_visitor{*this};

    template<class T>
    void visit_optional(boost::optional<T> const& o)
    {
        if(o) {
            visit(*o);
        }
    }

    template<class T>
    void visit_vector(std::vector<T> const& v)
    {
        for (auto const& n : v) {
            visit(n);
        }
    }

    template<class T>
    void visit_mult_binary_expr(T const& e)
    {
        visit(e->lhs);
        for (auto const& p : e->rhss) {
            visit(p.second);
        }
    }

    template<class T>
    void visit_binary_expr(T const& e)
    {
        visit(e->lhs);
        visit_vector(e->rhss);
    }

public:

    virtual ~ast_walker_base()
    {}

    // Note:
    // Member functions are virtual because calling member functions
    // in base class behaves correctly.
    //
    // struct X : public ast_walker_base {
    //     void visit(ast::node::program const& p)
    //     {
    //         // do something for node::program...
    //
    //         // call member function in base class
    //         ast_walker_base::visit(p);
    //
    //         // In ast_walker_base::visit(node::program), executing
    //         // 'visit(node::global_definition)' calls X::visit(node::global_definition)
    //         // properly if it is overridden.
    //     }
    // };







    virtual void visit(ast::node::character_literal const&)
    {}

    virtual void visit(ast::node::float_literal const&)
    {}

    virtual void visit(ast::node::boolean_literal const&)
    {}

    virtual void visit(ast::node::string_literal const&)
    {}

    virtual void visit(ast::node::integer_literal const&)
    {}

    virtual void visit(ast::node::array_literal const& al)
    {
        visit_vector(al->element_exprs);
    }

    virtual void visit(ast::node::tuple_literal const& tl)
    {
        visit_vector(tl->element_exprs);
    }

    virtual void visit(ast::node::symbol_literal const&)
    {}

    virtual void visit(ast::node::dict_literal const& dl)
    {
        for (auto const& p : dl->value) {
            visit(p.first);
            visit(p.second);
        }
    }

    virtual void visit(ast::node::literal const& l)
    {
        boost::apply_visitor(variant_visitor, l->value);
    }

    virtual void visit(ast::node::identifier const&)
    {}

    virtual void visit(ast::node::var_ref const& vr)
    {
        visit(vr->name);
    }

    virtual void visit(ast::node::parameter const& p)
    {
        visit(p->name);
        visit_optional(p->type);
    }

    virtual void visit(ast::node::function_call const& fc)
    {
        visit_vector(fc->args);
    }

    virtual void visit(ast::node::object_construct const& oc)
    {
        visit(oc->type);
        visit_vector(oc->args);
    }

    virtual void visit(ast::node::primary_expr const& pe)
    {
        boost::apply_visitor(variant_visitor, pe->value);
    }

    virtual void visit(ast::node::index_access const& ia)
    {
        visit(ia->index_expr);
    }

    virtual void visit(ast::node::member_access const& ma)
    {
        visit(ma->member_name);
    }

    virtual void visit(ast::node::postfix_expr const& ue)
    {
        visit(ue->prefix);
        for (auto const& p : ue->postfixes) {
            boost::apply_visitor(variant_visitor, p);
        }
    }

    virtual void visit(ast::node::unary_expr const& ue)
    {
        visit(ue->expr);
    }

    virtual void visit(ast::node::template_type const& tt)
    {
        visit(tt->template_name);
        if (tt->instantiated_types) {
            visit_vector(*(tt->instantiated_types));
        }
    }

    virtual void visit(ast::node::primary_type const& pt)
    {
        boost::apply_visitor(variant_visitor, pt->value);
    }

    virtual void visit(ast::node::array_type const& at)
    {
        visit(at->elem_type);
    }

    virtual void visit(ast::node::dict_type const& dt)
    {
        visit(dt->key_type);
        visit(dt->value_type);
    }

    virtual void visit(ast::node::tuple_type const& tt)
    {
        visit_vector(tt->arg_types);
    }

    virtual void visit(ast::node::func_type const& ft)
    {
        visit_vector(ft->arg_types);
        visit(ft->ret_type);
    }

    virtual void visit(ast::node::proc_type const& pt)
    {
        visit_vector(pt->arg_types);
    }

    virtual void visit(ast::node::compound_type const& ct)
    {
        boost::apply_visitor(variant_visitor, ct->value);
    }

    virtual void visit(ast::node::qualified_type const& qt)
    {
        visit(qt->type);
    }

    virtual void visit(ast::node::cast_expr const& ce)
    {
        visit_vector(ce->dest_types);
        visit(ce->source_expr);
    }

    virtual void visit(ast::node::mult_expr const& e)
    {
        visit_mult_binary_expr(e);
    }

    virtual void visit(ast::node::additive_expr const& e)
    {
        visit_mult_binary_expr(e);
    }

    virtual void visit(ast::node::shift_expr const& e)
    {
        visit_mult_binary_expr(e);
    }

    virtual void visit(ast::node::relational_expr const& e)
    {
        visit_mult_binary_expr(e);
    }

    virtual void visit(ast::node::equality_expr const& e)
    {
        visit_mult_binary_expr(e);
    }

    virtual void visit(ast::node::and_expr const& e)
    {
        visit_binary_expr(e);
    }

    virtual void visit(ast::node::xor_expr const& e)
    {
        visit_binary_expr(e);
    }

    virtual void visit(ast::node::or_expr const& e)
    {
        visit_binary_expr(e);
    }

    virtual void visit(ast::node::logical_and_expr const& e)
    {
        visit_binary_expr(e);
    }

    virtual void visit(ast::node::logical_or_expr const& e)
    {
        visit_binary_expr(e);
    }

    virtual void visit(ast::node::if_expr const& ie)
    {
        visit(ie->condition_expr);
        visit(ie->then_expr);
        visit(ie->else_expr);
    }

    virtual void visit(ast::node::range_expr const& re)
    {
        visit(re->lhs);
        if (re->maybe_rhs) {
            visit(re->maybe_rhs->second);
        }
    }

    virtual void visit(ast::node::compound_expr const& ce)
    {
        boost::apply_visitor(variant_visitor, ce->child_expr);
        visit_optional(ce->maybe_type);
    }

    virtual void visit(ast::node::variable_decl const& vd)
    {
        visit(vd->name);
        visit_optional(vd->maybe_type);
    }

    virtual void visit(ast::node::initialize_stmt const& is)
    {
        visit_vector(is->var_decls);
        if (is->maybe_rhs_exprs) {
            visit_vector(*(is->maybe_rhs_exprs));
        }
    }

    virtual void visit(ast::node::assignment_stmt const& as)
    {
        visit_vector(as->assignees);
        visit_vector(as->rhs_exprs);
    }

    virtual void visit(ast::node::if_stmt const& is)
    {
        visit(is->condition);
        visit(is->then_stmts);
        for (auto const& p : is->elseif_stmts_list) {
            visit(p.first);
            visit(p.second);
        }
        visit_optional(is->maybe_else_stmts);
    }

    virtual void visit(ast::node::return_stmt const& rs)
    {
        visit_vector(rs->ret_exprs);
    }

    virtual void visit(ast::node::case_stmt const& cs)
    {
        for (auto const& p : cs->when_stmts_list) {
            visit(p.first);
            visit(p.second);
        }
        visit_optional(cs->maybe_else_stmts);
    }

    virtual void visit(ast::node::switch_stmt const& ss)
    {
        visit(ss->target_expr);
        for (auto const& p : ss->when_stmts_list) {
            visit_vector(p.first);
            visit(p.second);
        }
        visit_optional(ss->maybe_else_stmts);
    }

    virtual void visit(ast::node::for_stmt const& fs)
    {
        visit_vector(fs->iter_vars);
        visit(fs->range_expr);
        visit(fs->body_stmts);
    }

    virtual void visit(ast::node::while_stmt const& ws)
    {
        visit(ws->condition);
        visit(ws->body_stmts);
    }

    virtual void visit(ast::node::postfix_if_stmt const& pif)
    {
        boost::apply_visitor(variant_visitor, pif->body);
        visit(pif->condition);
    }

    virtual void visit(ast::node::compound_stmt const& cs)
    {
        boost::apply_visitor(variant_visitor, cs->value);
    }

    virtual void visit(ast::node::statement_block const& sb)
    {
        visit_vector(sb->value);
    }

    virtual void visit(ast::node::function_definition const& fd)
    {
        visit(fd->name);
        for (auto const& p : fd->params) {
            visit(p);
        }
        visit_optional(fd->return_type);
        visit(fd->body);
        visit_optional(fd->ensure_body);
    }

    virtual void visit(ast::node::procedure_definition const& pd)
    {
        visit(pd->name);
        for (auto const& p : pd->params) {
            visit(p);
        }
        visit(pd->body);
        visit_optional(pd->ensure_body);
    }

    virtual void visit(ast::node::constant_decl const& cd)
    {
        visit(cd->name);
        visit_optional(cd->maybe_type);
    }

    virtual void visit(ast::node::constant_definition const& cd)
    {
        visit_vector(cd->const_decls);
        visit_vector(cd->initializers);
    }

    virtual void visit(ast::node::global_definition const& gd)
    {
        boost::apply_visitor(variant_visitor, gd->value);
    }

    virtual void visit(ast::node::program const& p)
    {
        visit_vector(p->inu);
    }

};

} // namespace helper

} // namespace dachs

#endif    // DACHS_HELPER_AST_WALKER_BASE_HPP_INCLUDED
