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

    void visit(ast::node::constant_definition const& const_def)
    {
        /*
         * XXX:
         * Below code causes SEGV because shared_ptr's ownership overs.
         */
        // auto maybe_global_scope = get<scope::global_scope>(current_scope);
        // assert(maybe_global_scope);
        // auto& global_scope = *maybe_global_scope;
        // for (auto const& decl : const_def->const_decls) {
        //     auto var = symbol::make<symbol::var_symbol>(decl->name->value);
        //     // TODO add symbol to AST node
        //     global_scope->define_global_constant(var);
        // }
    }

    void visit(ast::node::function_definition const& func_def)
    {
        /*
         * XXX:
         * Below code causes SEGV because shared_ptr's ownership overs.
         */
        // auto maybe_global_scope = get<scope::global_scope>(current_scope);
        // assert(maybe_global_scope);
        // auto& global_scope = *maybe_global_scope;
        // assert(global_scope);
        // auto new_func = make<func_scope>(global_scope, func_def->name->value);
        // // TODO add scope to AST node?
        // global_scope->define_function(new_func);
    }

    template<class T>
    void visit(T const&)
    {
        // Do nothing as default behavior
    }
};

} // namespace detail

scope_tree make_scope_tree(ast::ast &a)
{
    global_scope tree_root;
    detail::scope_tree_generator generator{tree_root};
    ast::walker<detail::scope_tree_generator> walker{generator};
    walker.walk(a.root);
    return scope_tree{tree_root};
}

std::string dump_scope_tree(scope_tree const&)
{
    return ""; // TODO
}

} // namespace scope
} // namespace dachs
