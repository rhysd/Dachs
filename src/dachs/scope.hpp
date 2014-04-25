#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <vector>
#include <boost/variant/variant.hpp>

#include "dachs/scope_fwd.hpp"
#include "dachs/symbol.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {

// Forward declarations to define node pointers
namespace scope_node {

struct global_scope;
struct local_scope;
struct func_scope;
struct class_scope;

} // namespace scope

// Dynamic resources to use actually
namespace scope {

using dachs::helper::make;

using any_scope
        = boost::variant< global_scope
                        , local_scope
                        , func_scope
                        , class_scope
                    >;

using enclosing_scope_type
        = boost::variant< weak_global_scope
                        , weak_local_scope
                        , weak_func_scope
                        , weak_class_scope
                    >;
} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

struct basic_scope {
    scope::enclosing_scope_type enclosing_scope;

    template<class AnyScope>
    explicit basic_scope(AnyScope const& parent) noexcept
        : enclosing_scope(parent)
    {}

    basic_scope() noexcept
    {}

    virtual ~basic_scope() noexcept
    {}
};

struct local_scope final : public basic_scope {
    std::vector<scope::local_scope> children;
    std::vector<symbol::var_symbol> local_vars;

    template<class AnyScope>
    explicit local_scope(AnyScope const& enclosing) noexcept
        : basic_scope(enclosing)
    {}

    void define_child(scope::local_scope const& child) noexcept
    {
        children.push_back(child);
    }

    void define_local_var(symbol::var_symbol const& new_var) noexcept
    {
        local_vars.push_back(new_var);
    }
};

// TODO: I should make proc_scope? It depends on return type structure.
struct func_scope final : public basic_scope, public symbol_node::basic_symbol {
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;

    template<class P>
    explicit func_scope(P const& p, std::string const& s) noexcept
        : basic_scope(p)
        , basic_symbol(s)
    {}

    void define_param(symbol::var_symbol const& new_var) noexcept
    {
        params.push_back(new_var);
    }
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::builtin_type_symbol> const builtin_type_symbols;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;

    global_scope() noexcept
        : basic_scope()
        , builtin_type_symbols({
                symbol::make<symbol::builtin_type_symbol>("int"),
                symbol::make<symbol::builtin_type_symbol>("float"),
                symbol::make<symbol::builtin_type_symbol>("char"),
                symbol::make<symbol::builtin_type_symbol>("string"),
                symbol::make<symbol::builtin_type_symbol>("symbol"),
            // {"dict"}
            // {"array"}
            // {"tuple"}
            // {"func"}
            // {"class"}
        })
    {}

    void define_function(scope::func_scope const& new_func) noexcept
    {
        functions.push_back(new_func);
    }

    void define_global_constant(symbol::var_symbol const& new_var) noexcept
    {
        const_symbols.push_back(new_var);
    }

    void define_class(scope::class_scope const& new_class) noexcept
    {
        classes.push_back(new_class);
    }
};

struct class_scope final : public basic_scope, public symbol_node::basic_symbol {
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;

    // std::vector<type> for instanciated types (if this isn't template, it should contains only one element)

    template<class P>
    explicit class_scope(P const& p, std::string const& name) noexcept
        : basic_scope(p)
        , basic_symbol(name)
    {}

    void define_member_func(scope::func_scope const& new_func) noexcept
    {
        member_func_scopes.push_back(new_func);
    }

    void define_member_var_symbols(symbol::member_var_symbol const& new_var) noexcept
    {
        member_var_symbols.push_back(new_var);
    }
};

} // namespace scope_node

namespace ast {
struct ast;
} // namespace ast

namespace scope {

struct scope_tree final {
    scope::global_scope root;

    explicit scope_tree(scope::global_scope const& r) noexcept
        : root(r)
    {}

    scope_tree()
        : root{}
    {}
};

scope_tree make_scope_tree(ast::ast &ast);

} // namespace scope
} // namespace dachs

#endif    // DACHS_SCOPE_HPP_INCLUDED
