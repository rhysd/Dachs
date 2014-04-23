#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <vector>
#include <boost/variant/variant.hpp>

#include "scope_fwd.hpp"
#include "dachs/symbol.hpp"

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

template<class Scope, class... Args>
inline Scope make(Args &&... args)
{
    return std::make_shared<typename Scope::element_type>(std::forward<Args>(args)...);
}

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
    explicit basic_scope(AnyScope const& parent)
        : enclosing_scope(parent)
    {}

    basic_scope()
    {}

    virtual ~basic_scope()
    {}
};

struct local_scope final : public basic_scope {
    std::vector<scope::local_scope> children;
    std::vector<symbol::var_symbol> local_vars;

    template<class AnyScope>
    explicit local_scope(AnyScope const& enclosing)
        : basic_scope(enclosing)
    {}

    void define_child(scope::local_scope const& child)
    {
        children.push_back(child);
    }

    void define_local_var(symbol::var_symbol const& new_var)
    {
        local_vars.push_back(new_var);
    }
};

// TODO: I should make proc_scope? It depends on return type structure.
struct func_scope final : public basic_scope {
    symbol::func_symbol name;
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;

    template<class P>
    explicit func_scope(P const& p, symbol::func_symbol const& s)
        : basic_scope(p)
        , name(s)
    {}

    void define_param(symbol::var_symbol const& new_var)
    {
        params.push_back(new_var);
    }
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::builtin_type_symbol> const builtin_type_symbols;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;

    global_scope()
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
        })
    {}

    void define_function(scope::func_scope const& new_func)
    {
        functions.push_back(new_func);
    }

    void define_global_constant(symbol::var_symbol const& new_var)
    {
        const_symbols.push_back(new_var);
    }

    void define_class(scope::class_scope const& new_class)
    {
        classes.push_back(new_class);
    }
};

struct class_scope final : public basic_scope {
    symbol::class_symbol symbol;
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;

    template<class P>
    explicit class_scope(P const& p, symbol::class_symbol const& name)
        : basic_scope(p)
        , symbol(name)
    {}

    void define_member_func(scope::func_scope const& new_func)
    {
        member_func_scopes.push_back(new_func);
    }

    void define_member_var_symbols(symbol::member_var_symbol const& new_var)
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

    explicit scope_tree(scope::global_scope const& r)
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
