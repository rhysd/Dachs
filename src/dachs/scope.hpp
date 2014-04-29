#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <vector>
#include <memory>
#include <cassert>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

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

namespace detail {

struct func_resolver : public boost::static_visitor<boost::optional<scope::func_scope>> {
    std::string const& name;

    explicit func_resolver(std::string const& n) noexcept
        : name(n)
    {}

    template<class WeakScope>
    result_type operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock()->resolve_func(name);
    }
};

struct class_resolver : public boost::static_visitor<boost::optional<scope::class_scope>> {
    std::string const& name;

    explicit class_resolver(std::string const& n) noexcept
        : name(n)
    {}

    template<class WeakScope>
    result_type operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock()->resolve_class(name);
    }
};

struct var_resolver : public boost::static_visitor<boost::optional<symbol::var_symbol>> {
    std::string const& name;

    explicit var_resolver(std::string const& n) noexcept
        : name(n)
    {}

    template<class WeakScope>
    result_type operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock()->resolve_var(name);
    }
};

} // namespace detail

struct basic_scope {
    // Note:
    // I don't use base class pointer to check the parent is alive or not.
    // Or should I?
    scope::enclosing_scope_type enclosing_scope;

    template<class AnyScope>
    explicit basic_scope(AnyScope const& parent) noexcept
        : enclosing_scope(parent)
    {}

    basic_scope() noexcept
    {}

    virtual ~basic_scope() noexcept
    {}

    // TODO resolve member variables and member functions

    virtual boost::optional<scope::func_scope> resolve_func(std::string const& name) const
    {
        return boost::apply_visitor(detail::func_resolver{name}, enclosing_scope);
    }

    virtual boost::optional<scope::class_scope> resolve_class(std::string const& name) const
    {
        return boost::apply_visitor(detail::class_resolver{name}, enclosing_scope);
    }

    virtual boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const
    {
        return boost::apply_visitor(detail::var_resolver{name}, enclosing_scope);
    }
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

    boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        for (auto const& s : local_vars) {
            if (s->name == name) {
                return s;
            }
        }
        return boost::apply_visitor(detail::var_resolver{name}, enclosing_scope);
    }
};

// TODO: I should make proc_scope? It depends on return type structure.
struct func_scope final : public basic_scope, public symbol_node::basic_symbol {
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;
    // std::vector<template_type> template_type_symbols;

    // std::vector<type> for instanciated types (if this isn't template, it should contains only one element)

    template<class P>
    explicit func_scope(P const& p, std::string const& s) noexcept
        : basic_scope(p)
        , basic_symbol(s)
    {}

    void define_param(symbol::var_symbol const& new_var) noexcept
    {
        params.push_back(new_var);
    }

    virtual boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        for (auto const& p : params) {
            if (p->name == name) {
                return p;
            }
        }
        return boost::apply_visitor(detail::var_resolver{name}, enclosing_scope);
    }
};

struct class_scope final : public basic_scope, public symbol_node::basic_symbol {
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;
    // std::vector<template_type> template_type_symbols;

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

    // TODO: Resolve member functions and member variables
};

struct global_scope final : public basic_scope
                          , public std::enable_shared_from_this<global_scope> {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;

    global_scope() noexcept
        : basic_scope()
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

    virtual boost::optional<scope::func_scope> resolve_func(std::string const& name) const
    {
        for( auto const& f : functions ) {
            if (f->name == name) {
                return f;
            }
        }
        return boost::none;
    }

    virtual boost::optional<scope::class_scope> resolve_class(std::string const& name) const
    {
        for( auto const& c : classes ) {
            if (c->name == name) {
                return c;
            }
        }
        return boost::none;
    }

    virtual boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const
    {
        for( auto const& v : const_symbols ) {
            if (v->name == name) {
                return v;
            }
        }
        return boost::none;
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
