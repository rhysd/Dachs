#if !defined DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
#define      DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED

#include <iterator>
#include <cstddef>
#include <cassert>

#include <boost/format.hpp>

#include "dachs/semantics/analyzer_common.hpp"
#include "dachs/semantics/forward_analyzer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::apply_lambda;

// Walk to analyze functions, classes and member variables symbols to make forward reference possible
class forward_symbol_analyzer {

    scope::any_scope current_scope;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker>
    void with_new_scope(Scope && new_scope, Walker const& walker)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        walker();
        current_scope = tmp_scope;
    }

    template<class Node, class Message>
    void semantic_error(Node const& n, Message const& msg) noexcept
    {
        output_semantic_error(n, msg);
        failed++;
    }

    template<class Node>
    std::string get_lambda_name(Node const& n) const noexcept
    {
        return "lambda."
            + std::to_string(n->line)
            + '.' + std::to_string(n->col)
            + '.' + std::to_string(n->length);
    }

public:

    size_t failed;

    template<class Scope>
    explicit forward_symbol_analyzer(Scope const& s) noexcept
        : current_scope(s), failed(0)
    {}

    template<class Walker>
    void visit(ast::node::statement_block const& block, Walker const& recursive_walker)
    {
        auto new_local_scope = scope::make<scope::local_scope>(current_scope);
        block->scope = new_local_scope;
        if (auto maybe_local_scope = get_as<scope::local_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_local_scope;
            enclosing_scope->define_child(new_local_scope);
        } else if (auto maybe_func_scope = get_as<scope::func_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_func_scope;
            enclosing_scope->body = new_local_scope;
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
        with_new_scope(std::move(new_local_scope), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& recursive_walker)
    {
        // Define scope
        auto new_func = scope::make<scope::func_scope>(func_def, current_scope, func_def->name);
        new_func->type = type::make<type::generic_func_type>(scope::weak_func_scope{new_func});
        func_def->scope = new_func;

        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            semantic_error(func_def, boost::format("Procedure '%1%' can't have return type") % func_def->name);
            return;
        }

        // Note:
        // Get return type for checking duplication of overloaded function
        if (func_def->return_type) {
            auto ret_type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, *func_def->return_type);
            func_def->ret_type = ret_type;
            new_func->ret_type = ret_type;
        }

        if (auto maybe_global_scope = get_as<scope::global_scope>(current_scope)) {
            auto& global_scope = *maybe_global_scope;
            auto new_func_var = symbol::make<symbol::var_symbol>(func_def, func_def->name, true /*immutable*/);
            new_func_var->type = new_func->type;
            global_scope->define_function(new_func);
            global_scope->define_global_function_constant(new_func_var);
        } else if (auto maybe_local_scope = get_as<scope::local_scope>(current_scope)) {
            (*maybe_local_scope)->define_unnamed_func(new_func);
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        with_new_scope(std::move(new_func), recursive_walker);
    }

    auto get_param_sym(ast::node::parameter const& param)
    {
        // Note:
        // When the param's name is "_", it means unused.
        // Unique number (the address of 'param') is used instead of "_" as its name.
        // This is because "_" variable should be ignored by symbol resolution and
        // duplication check; it means that duplication of "_" must be permitted.
        // Defining the symbol is not skipped because of overload resolution. Parameters
        // must have its symbol and type for overloading the function.
        auto const new_param_sym =
            symbol::make<symbol::var_symbol>(
                param,
                param->name == "_" ? std::to_string(reinterpret_cast<size_t>(param.get())) : param->name,
                !param->is_var
            );
        param->param_symbol = new_param_sym;

        if (param->param_type) {
            // XXX:
            // type_calculator requires class information which should be analyzed forward.
            param->type =
                boost::apply_visitor(
                    type_calculator_from_type_nodes{current_scope},
                    *(param->param_type)
                );
            new_param_sym->type = param->type;
        }

        return new_param_sym;
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& recursive_walker)
    {
        if (auto maybe_func = get_as<scope::func_scope>(current_scope)) {

            auto const new_param_sym = get_param_sym(param);

            if (!param->param_type) {
                param->type = type::make<type::template_type>(param);
                new_param_sym->type = param->type;
            }
            if (!(*maybe_func)->define_param(new_param_sym)) {
                failed++;
                return;
            }

        } else if (auto maybe_local = get_as<scope::local_scope>(current_scope)) {
            // Note:
            // Enter here when the param is a variable to iterate in 'for' statement

            // XXX:
            // Do nothing
            // Symbol is defined in analyzer::visit(ast::node::for_stmt) for 'for' statement.
            // This is because it requires a range of for to get a type of variable to iterate.

        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::for_stmt const& for_, Walker const& recursive_walker)
    {
        recursive_walker();

        assert(!for_->body_stmts->scope.expired());
        auto const child_scope = for_->body_stmts->scope.lock();

        for (auto const& i : for_->iter_vars) {
            assert(i->param_symbol.expired());
            auto const sym = get_param_sym(i);
            if (!child_scope->define_variable(sym)) {
                failed++;
                return;
            }
        }
    }

    template<class Node>
    void visit_do_block(Node const& n)
    {
        if (n->do_block) {
            auto &b = *n->do_block;
            b->name = get_lambda_name(b);
            ast::walk_topdown(b, *this);
        }
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& recursive_walker)
    {
        recursive_walker();
        visit_do_block(invocation);
    }

    template<class Walker>
    void visit(ast::node::ufcs_invocation const& ufcs, Walker const& recursive_walker)
    {
        recursive_walker();
        visit_do_block(ufcs);
    }

    // TODO: class scopes and member function scopes

    template<class T, class Walker>
    void visit(T const&, Walker const& walker)
    {
        // Simply visit children recursively
        walker();
    }

};

} // namespace detail

template<class Node, class Scope>
std::size_t dispatch_forward_analyzer(Node &node, Scope const& scope_root)
{
    // Generate scope tree
    detail::forward_symbol_analyzer forward_resolver{scope_root};
    ast::walk_topdown(node, forward_resolver);

    return forward_resolver.failed;
}

template<class Scope>
std::size_t check_functions_duplication(Scope const& scope_root)
{
    std::size_t failed = 0u;
    auto const end = scope_root->functions.cend();
    for (auto left = scope_root->functions.cbegin(); left != end; ++left) {
        for (auto right = std::next(left); right != end; ++right) {
            if (**right == **left) {
                auto const rhs_def = (*right)->get_ast_node();
                auto const lhs_def = (*left)->get_ast_node();
                output_semantic_error(rhs_def, boost::format("'%1%' is redefined.\nNote: Previous definition is at line:%2%, col:%3%") % (*right)->to_string() % lhs_def->line % lhs_def->col);
                failed++;
            }
        }
    }

    return failed;
}

// TODO:
// Consider class scope.  Now global scope is only considered.
template<class Node, class Scope>
Scope analyze_ast_node_forward(Node &node, Scope const& scope_root)
{
    {
        std::size_t const failed = dispatch_forward_analyzer(node, scope_root);
        if (failed > 0) {
            throw dachs::semantic_check_error{failed, "forward symbol resolution"};
        }
    }

    {
        std::size_t const failed = check_functions_duplication(scope_root);
        if (failed > 0) {
            throw dachs::semantic_check_error{failed, "function duplication check"};
        }
    }

    return scope_root;
}

} // namespace semantics
} // namespace dachs


#endif    // DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
