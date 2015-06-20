#include "dachs/semantics/forward_analyzer_impl.hpp"

namespace dachs {
namespace semantics {
namespace detail {

template<class T>
inline auto make_global_func_param(char const* const name, T && type)
{
    auto p = symbol::make<symbol::var_symbol>(nullptr, name, true, true);
    p->type = std::forward<T>(type);
    return p;
}

template<class S, class T>
inline auto make_global_func(S const& scope, char const* const name, T && ret)
{
    auto func = scope::make<scope::func_scope>(nullptr, scope, name, true);
    func->body = scope::make<scope::local_scope>(func);
    func->ret_type = std::forward<T>(ret);
    auto func_var = symbol::make<symbol::var_symbol>(nullptr, name, true, true);
    func_var->type = type::make<type::generic_func_type>(func);
    func_var->is_global = true;
    scope->define_function(func);
    scope->force_define_constant(std::move(func_var));
    return func;
}

} // namespace detail

scope::scope_tree analyze_symbols_forward(ast::ast &a, syntax::importer &i)
{
    auto const scope_root = scope::make<scope::global_scope>(a.root);

    {
        auto dummy_template_type = type::make<type::template_type>(a.root);
        // Builtin functions

        {
            // func print(str)
            auto print_func = detail::make_global_func(scope_root, "print", type::get_unit_type());
            print_func->define_param(detail::make_global_func_param("value", dummy_template_type));
        }

        {
            // func println(str)
            auto println_func = detail::make_global_func(scope_root, "println", type::get_unit_type());
            println_func->define_param(detail::make_global_func_param("value", dummy_template_type));
        }

        {
            // func read_cycle_counter() : uint
            detail::make_global_func(scope_root, "__builtin_read_cycle_counter", type::get_builtin_type("uint"));
        }

        {
            // func address_of(x)
            auto address_of_func = detail::make_global_func(scope_root, "__builtin_address_of", type::get_builtin_type("uint"));
            address_of_func->define_param(detail::make_global_func_param("ptr", dummy_template_type));
        }

        {
            // func getchar() : char
            detail::make_global_func(scope_root, "__builtin_getchar", type::get_builtin_type("char"));
        }

        {
            // func fatal()
            detail::make_global_func(scope_root, "fatal", type::get_unit_type());

            // func fatal(reason)
            auto fatal_func = detail::make_global_func(scope_root, "fatal", type::get_unit_type());
            fatal_func->define_param(detail::make_global_func_param("reason", dummy_template_type));
        }

        {
            // func null?(p : pointer)
            auto is_null_func = detail::make_global_func(scope_root, "__builtin_null?", type::get_builtin_type("bool"));
            is_null_func->define_param(detail::make_global_func_param("ptr", type::make<type::pointer_type>(dummy_template_type)));
        }

        {
            // func realloc(p : pointer, uint new_size)
            auto pointer_type = type::make<type::pointer_type>(dummy_template_type);
            auto realloc_func = detail::make_global_func(scope_root, "__builtin_realloc", pointer_type);
            realloc_func->define_param(detail::make_global_func_param("ptr", pointer_type));
            realloc_func->define_param(detail::make_global_func_param("new_size", *type::get_builtin_type("uint")));
        }

        {
            // func free(ptr)
            auto free_func = detail::make_global_func(scope_root, "__builtin_free", type::get_unit_type());
            free_func->define_param(detail::make_global_func_param("ptr", dummy_template_type));
        }

        {
            // func gen_symbol(ptr, size)
            auto gen_symbol_func = detail::make_global_func(scope_root, "__builtin_gen_symbol", type::get_builtin_type("symbol"));
            gen_symbol_func->define_param(detail::make_global_func_param("ptr", type::make<type::pointer_type>(*type::get_builtin_type("char"))));
            gen_symbol_func->define_param(detail::make_global_func_param("size", *type::get_builtin_type("uint")));
        }

        {
            // func __builtin_gc_enable()
            detail::make_global_func(scope_root, "__builtin_enable_gc", type::get_unit_type());
            // func __builtin_gc_disable()
            detail::make_global_func(scope_root, "__builtin_disable_gc", type::get_unit_type());
            // func __builtin_gc_is_disabled()
            detail::make_global_func(scope_root, "__builtin_gc_disabled?", *type::get_builtin_type("bool"));
        }

        // Operators
        // cast functions
    }

    // Note:
    // Check function duplication after generating scope tree because overload resolution requires
    // of arguments and types of arguments require class information which should be forward-analyzed.

    return scope::scope_tree{analyze_ast_node_forward(a.root, scope_root, i)};
}

} // namespace semantics
} // namespace dachs

