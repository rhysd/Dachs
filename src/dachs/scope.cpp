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
class scope_tree_generator {

    any_scope current_scope;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker>
    void with_new_scope(Scope && new_scope, Walker const& f)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        f();
        current_scope = tmp_scope;
    }

public:

    explicit scope_tree_generator(any_scope const& s)
        : current_scope(s)
    {}

    template<class Walker>
    void visit(ast::node::statement_block const& /*block*/, Walker const& recursive_walker)
    {
        // TODO add symbols
        auto new_local_scope = make<local_scope>(current_scope);
        if (auto maybe_local_scope = get<local_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_local_scope;
            enclosing_scope->define_child(new_local_scope);
        } else if (auto maybe_func_scope = get<func_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_func_scope;
            enclosing_scope->body = new_local_scope;
        } else {
            assert(false); // Should not reach here
        }
        with_new_scope(std::move(new_local_scope), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::constant_definition const& const_def, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        for (auto const& decl : const_def->const_decls) {
            auto var = symbol::make<symbol::var_symbol>(decl->name->value);
            global_scope->define_global_constant(var);
        }
        recursive_walker(); // Note: walk recursively for lambda expression(it doesn't exist yet)
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto const func_name = symbol::make<symbol::func_symbol>(func_def->name->value);
        auto new_func = make<func_scope>(global_scope, func_name);
        global_scope->define_function(new_func);
        with_new_scope(std::move(new_func), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::procedure_definition const& proc_def, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto const proc_name = symbol::make<symbol::func_symbol>(proc_def->name->value);
        auto new_proc = make<func_scope>(global_scope, proc_name);
        global_scope->define_function(new_proc);
        with_new_scope(std::move(new_proc), recursive_walker);
    }

    template<class T, class Walker>
    void visit(T const&, Walker const& f)
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

} // namespace scope
} // namespace dachs
