#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <vector>
#include <type_traits>
#include <cassert>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include "dachs/scope_fwd.hpp"
#include "dachs/symbol.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {

// Dynamic resources to use actually
namespace scope {

using dachs::helper::make;

} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

namespace detail {

// FIXME: reduce duplicate of below classes

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

struct template_type_resolver : public boost::static_visitor<boost::optional<symbol::template_type_symbol>> {
    std::string const& name;

    explicit template_type_resolver(std::string const& n) noexcept
        : name(n)
    {}

    template<class WeakScope>
    result_type operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock()->resolve_template_type(name);
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

    template<class Node1, class Node2>
    void print_duplication_error(Node1 const& node1, Node2 const& node2, std::string const& name)
    {
        std::cerr << "Semantic error at line:" << node1->line << ", col:" << node1->col << "\nSymbol '" << name << "' is redefined.\nPrevious definition is at line:" << node2->line << ", col:" << node2->col << std::endl;
    }

    template<class Symbol
           , class = typename std::enable_if<
                std::is_base_of<
                    symbol_node::basic_symbol
                    , typename Symbol::element_type
                >::value
            >::type
        >
    bool define_symbol(std::vector<Symbol> &container, Symbol const& symbol)
    {
        if (auto maybe_duplication = helper::find(container, symbol)) {
            print_duplication_error(symbol->ast_node.get_shared(), (*maybe_duplication)->ast_node.get_shared(), symbol->name);
            return false;
        }
        // TODO: raise warning when a variable shadows other variables
        container.push_back(symbol);
        return true;
    }

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

    virtual boost::optional<symbol::template_type_symbol> resolve_template_type(std::string const& var_name) const
    {
        return boost::apply_visitor(detail::template_type_resolver{var_name}, enclosing_scope);
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

    bool define_local_var(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(local_vars, new_var);
    }

    boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        auto const target_var = helper::find_if(local_vars, [&name](auto const& v){ return v->name == name; });
        return target_var ? *target_var : boost::apply_visitor(detail::var_resolver{name}, enclosing_scope);
    }
};

// TODO: I should make proc_scope? It depends on return type structure.
struct func_scope final : public basic_scope, public symbol_node::basic_symbol {
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;
    std::vector<symbol::template_type_symbol> templates;

    template<class Node, class P>
    explicit func_scope(Node const& n, P const& p, std::string const& s) noexcept
        : basic_scope(p)
        , basic_symbol(n, s)
    {}

    bool define_param(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(params, new_var);
    }

    void define_template_param(symbol::template_type_symbol const& new_template) noexcept
    {
        // Note: No need to check duplication bacause param is already sure to be unique
        templates.push_back(new_template);
    }

    boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        auto const target_var = helper::find_if(params, [&name](auto const& v){ return v->name == name; });
        return target_var ? *target_var : boost::apply_visitor(detail::var_resolver{name}, enclosing_scope);
    }

    boost::optional<symbol::template_type_symbol> resolve_template_type(std::string const& var_name) const override
    {
        return helper::find_if(templates, [&var_name](auto const& t){ return t->name == var_name; });
        // Do not recursive because functions aren't allowed to nest
    }
};

struct class_scope final : public basic_scope, public symbol_node::basic_symbol {
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;
    std::vector<symbol::template_type_symbol> templates;

    // std::vector<type> for instanciated types (if this isn't template, it should contains only one element)

    template<class Node, class Parent>
    explicit class_scope(Node const& ast_node, Parent const& p, std::string const& name) noexcept
        : basic_scope(p)
        , basic_symbol(ast_node, name)
    {}

    bool define_member_func(scope::func_scope const& new_func) noexcept
    {
        return define_symbol(member_func_scopes, new_func);
    }

    bool define_member_var_symbols(symbol::member_var_symbol const& new_var) noexcept
    {
        return define_symbol(member_var_symbols, new_var);
    }

    void define_template_param(symbol::template_type_symbol const& new_template) noexcept
    {
        // Note: No need to check duplication bacause param is already sure to be unique
        templates.push_back(new_template);
    }

    // TODO: Resolve member functions and member variables
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;

    global_scope() noexcept
        : basic_scope()
    {}

    bool define_function(scope::func_scope const& new_func) noexcept
    {
        return define_symbol(functions, new_func);
    }

    bool define_global_constant(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(const_symbols, new_var);
    }

    bool define_class(scope::class_scope const& new_class) noexcept
    {
        return define_symbol(classes, new_class);
    }

    boost::optional<scope::func_scope> resolve_func(std::string const& name) const override
    {
        return helper::find_if(functions, [&name](auto const& f){ return f->name == name; });
    }

    boost::optional<scope::class_scope> resolve_class(std::string const& name) const override
    {
        return helper::find_if(classes, [&name](auto const& c){ return c->name == name; });
    }

    boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        return helper::find_if(const_symbols, [&name](auto const& v){ return v->name == name; });
    }

    boost::optional<symbol::template_type_symbol> resolve_template_type(std::string const&) const override
    {
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
