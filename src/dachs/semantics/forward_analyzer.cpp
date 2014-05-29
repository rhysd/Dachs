#include <cstddef>
#include <cassert>

#include "dachs/semantics/analyzer_common.hpp"
#include "dachs/semantics/forward_analyzer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/exception.hpp"
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
            assert(false); // Should not reach here
        }
        with_new_scope(std::move(new_local_scope), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& recursive_walker)
    {
        // Define scope
        auto maybe_global_scope = get_as<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_func = scope::make<scope::func_scope>(func_def, global_scope, func_def->name);
        new_func->type = type::make<type::func_ref_type>(scope::weak_func_scope{new_func});
        func_def->scope = new_func;

        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            semantic_error(func_def, boost::format("Procedure '%1%' can't have return type") % func_def->name);
            return;
        }

        if (func_def->return_type) {
            auto const& ret_type_node = *func_def->return_type;
            func_def->ret_type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, ret_type_node);
        }

        // Note:
        // Get type of parameter for checking duplication of overloaded function
        for (auto const& p : func_def->params) {
            if (p->param_type) {
                // XXX:
                // type_calculator requires class information which should be analyzed forward.
                // However
                p->type =
                    boost::apply_visitor(
                        type_calculator_from_type_nodes{current_scope},
                        *(p->param_type)
                    );
            }
        }

        auto new_func_var = symbol::make<symbol::var_symbol>(func_def, func_def->name);
        new_func_var->type = new_func->type;
        if (global_scope->define_function(new_func, func_def)) {
            // If the symbol passes duplication check, it is also defined as variable
            global_scope->define_global_function_constant(new_func_var);
            with_new_scope(std::move(new_func), recursive_walker);
        } else {
            failed++;
        }
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

scope::scope_tree analyze_symbols_forward(ast::ast &a)
{
    auto const tree_root = scope::make<scope::global_scope>();

    {
        // Builtin functions

        // func print(str)
        auto print_func = scope::make<scope::func_scope>(a.root, tree_root, "print");
        print_func->body = scope::make<scope::local_scope>(print_func);
        // Note: These definitions are never duplicate
        print_func->define_param(symbol::make<symbol::var_symbol>(a.root, "value"));
        tree_root->define_builtin_function(print_func);
        tree_root->define_global_constant(symbol::make<symbol::var_symbol>(a.root, "print"));

        // Operators
        // cast functions
    }

    {
        // Builtin classes

        // range
    }

    detail::forward_symbol_analyzer forward_resolver{tree_root};
    ast::walk_topdown(a.root, forward_resolver);

    if (forward_resolver.failed > 0) {
        throw dachs::semantic_check_error{forward_resolver.failed, "forward symbol resolution"};
    }

    return scope::scope_tree{tree_root};
}

} // namespace semantics
} // namespace dachs

