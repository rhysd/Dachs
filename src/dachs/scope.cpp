#include <cassert>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/scope.hpp"
#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"
#include "dachs/ast_walker.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace scope {

namespace detail {

using dachs::helper::variant::get;

// Walk to generate a scope tree
struct scope_tree_generator {

    any_scope current_scope;

    template<class F>
    void visit(ast::node::constant_definition const& const_def, F const& walk_recursive)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        for (auto const& decl : const_def->const_decls) {
            auto var = symbol::make<symbol::var_symbol>(decl->name->value);
            // TODO add symbol to AST node
            global_scope->define_global_constant(var);
        }
        walk_recursive();
    }

    template<class F>
    void visit(ast::node::function_definition const& func_def, F const& walk_recursive)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        assert(global_scope);
        auto new_func = make<func_scope>(global_scope, func_def->name->value);
        // TODO add scope to AST node?
        global_scope->define_function(new_func);
        current_scope = new_func;
        walk_recursive();
        current_scope = global_scope; // restore scope
    }

    template<class T, class F>
    void visit(T const&, F const& f)
    {
        // simply visit children recursively
        f();
    }
};

} // namespace detail

scope_tree make_scope_tree(ast::ast &a)
{
    auto tree_root = make<global_scope>();
    detail::scope_tree_generator generator{tree_root};
    auto walker = ast::make_walker(generator);
    walker.walk(a.root);
    return scope_tree{tree_root};
}

std::string dump_scope_tree(scope_tree const&)
{
    return ""; // TODO
}

} // namespace scope
} // namespace dachs
