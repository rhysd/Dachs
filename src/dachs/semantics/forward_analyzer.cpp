#include "dachs/semantics/forward_analyzer_impl.hpp"

namespace dachs {
namespace semantics {

scope::scope_tree analyze_symbols_forward(ast::ast &a)
{
    auto const scope_root = scope::make<scope::global_scope>(a.root);

    {
        // Builtin functions

        // func print(str)
        auto print_func = scope::make<scope::func_scope>(a.root, scope_root, "print", true);
        print_func->body = scope::make<scope::local_scope>(print_func);
        // Note: These definitions are never duplicate
        print_func->define_param(symbol::make<symbol::var_symbol>(a.root, "value", true, true));
        scope_root->define_function(print_func);
        scope_root->define_variable(symbol::make<symbol::var_symbol>(a.root, "print", true, true));

        // Operators
        // cast functions
    }

    {
        // Builtin classes

        // range
    }
        // Note:
        // Check function duplication after generating scope tree because overload resolution requires
        // of arguments and types of arguments require class information which should be forward-analyzed.

    return scope::scope_tree{analyze_ast_node_forward(a.root, scope_root)};
}

} // namespace semantics
} // namespace dachs

