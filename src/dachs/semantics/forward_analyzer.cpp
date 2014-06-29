#include "dachs/semantics/forward_analyzer_impl.hpp"

namespace dachs {
namespace semantics {

scope::scope_tree analyze_symbols_forward(ast::ast &a)
{
    auto const scope_root = scope::make<scope::global_scope>(a.root);

    {
        auto dummy_template_type = type::make<type::template_type>(a.root);
        auto unit_type = type::make<type::tuple_type>();
        // Builtin functions

        // func print(str)
        auto print_func = scope::make<scope::func_scope>(a.root, scope_root, "print", true);
        print_func->body = scope::make<scope::local_scope>(print_func);
        print_func->ret_type = unit_type;
        // Note: These definitions are never duplicate
        auto p = symbol::make<symbol::var_symbol>(a.root, "value", true, true);
        p->type = dummy_template_type;
        print_func->define_param(p);
        scope_root->define_function(print_func);
        auto func_var_sym = symbol::make<symbol::var_symbol>(a.root, "print", true, true);
        func_var_sym->type = type::make<type::func_ref_type>(print_func);
        scope_root->define_global_function_constant(func_var_sym);

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

