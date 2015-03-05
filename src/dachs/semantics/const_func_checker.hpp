#if !defined DACHS_SEMANTICS_CONST_FUNC_CHECKER_HPP_INCLUDED
#define      DACHS_SEMANTICS_CONST_FUNC_CHECKER_HPP_INCLUDED

#include <typeinfo>

#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using helper::variant::get_as;
using helper::variant::apply_lambda;

class const_member_func_checker {

    bool is_const_func = true;
    scope::func_scope const& scope;
    ast::node::function_definition &def;

    void visit_lhs_of_assign(ast::node::ufcs_invocation const& invocation) noexcept
    {
        if (invocation->is_instance_var_access()) {
            if (auto const child_ufcs = get_as<ast::node::ufcs_invocation>(invocation->child)) {
                visit_lhs_of_assign(*child_ufcs);
            } else if (auto const var = get_as<ast::node::var_ref>(invocation->child)) {
                assert(!(*var)->symbol.expired());
                if ((*var)->symbol.lock()->type == scope->params[0]->type) {
                    is_const_func = false;
                }
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    template<class Node>
    void visit_lhs_of_assign(Node const&) noexcept
    {
        // Note: Do nothing
    }

public:

    const_member_func_checker(decltype(scope) const& s, decltype(def) const& d) noexcept
        : scope(s), def(d)
    {
        assert(!s->is_template());
    }

    template<class Invocation>
    void visit_invocation(Invocation const& invocation) noexcept
    {
        assert(!invocation->callee_scope.expired());
        auto const callee = invocation->callee_scope.lock();
        if (callee == scope) {
            return;
        }

        auto const receiver = callee->resolve_receiver();
        if (!receiver) {
            return;
        }

        if (scope->params[0]->type != (*receiver)->type) {
            // Note:
            // Check if the callee function is a member function of the same class
            // as this function's.
            return;
        }

        if (!callee->is_const_) {
            // Note: Not determined yet if the callee is const or not.
            auto callee_def = callee->get_ast_node();
            const_member_func_checker checker{callee, callee_def};
            callee->is_const_ = checker.check_const();
        }

        is_const_func = callee->is_const();
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const&) noexcept
    {
        visit_invocation(invocation);
    }

    template<class Walker>
    void visit(ast::node::ufcs_invocation const& invocation) noexcept
    {
        if (!invocation->is_instance_var_access()) {
            visit_invocation(invocation);
        }
    }

    template<class Walker>
    void visit(ast::node::assignment_stmt const& assign, Walker const& w) noexcept
    {
        for (auto const& e : assign->assignees) {
            apply_lambda(
                    [this](auto const& node){ visit_lhs_of_assign(node); }
                    , e
                );
        }

        w(assign->rhs_exprs);
    }

    template<class Node, class Walker>
    void visit(Node const&, Walker const& w) noexcept
    {
        if (!is_const_func) {
            // Note: Already it resulted in non-const function.
            return;
        }
        w();
    }

    bool check_const() noexcept
    {
        assert(!scope->is_const_);
        if (!scope->is_member_func || scope->is_ctor()) {
            return false;
        }
        assert(!scope->params.empty());

        is_const_func = true;
        ast::walk_topdown(def, *this);
        return is_const_func;
    }
};

class const_func_invocation_checker {
    boost::optional<symbol::var_symbol> result = boost::none;

public:

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const&) noexcept
    {
        assert(!var->symbol.expired());
        auto const sym = var->symbol.lock();
        if (sym->immutable) {
            result = sym;
        }
    }

    template<class Walker>
    void visit(ast::node::index_access const& access, Walker const& w) noexcept
    {
        // XXX:
        // Too ad-hoc.
        // Do not consider variable reference in index.
        w(access->child);
    }

    template<class T, class W>
    void visit(T const&, W const& w) noexcept
    {
        w();
    }

    void apply(ast::node::func_invocation &invocation) noexcept
    {
        assert(!invocation->args.empty());
        ast::walk_topdown(invocation->args[0], *this);
    }

    void apply(ast::node::ufcs_invocation &invocation) noexcept
    {
        ast::walk_topdown(invocation->child, *this);
    }

    void apply(ast::node::binary_expr &expr) noexcept
    {
        ast::walk_topdown(expr->lhs, *this);
    }

    void apply(ast::node::index_access &access) noexcept
    {
        ast::walk_topdown(access->child, *this);
    }

    auto get_result() const noexcept
    {
        return result;
    }
};

template<class Invocation>
boost::optional<symbol::var_symbol> is_const_violated_invocation(Invocation invocation)
{
    auto const callee = invocation->callee_scope.lock();
    if (!callee->is_member_func || callee->is_const()) {
        return boost::none;
    }

    const_func_invocation_checker checker;
    checker.apply(invocation);
    return checker.get_result();
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_CONST_FUNC_CHECKER_HPP_INCLUDED
