#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <vector>
#include <type_traits>
#include <iostream>
#include <cstddef>
#include <cassert>
#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/format.hpp>

#include "dachs/scope_fwd.hpp"
#include "dachs/symbol.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {

// TODO: Move to proper place
template<class Message>
inline void output_semantic_error(std::size_t const line, std::size_t const col, Message const& msg, std::ostream &ost = std::cerr)
{
    ost << "Semantic error at line:" << line << ", col:" << col << '\n' << msg << std::endl;
}

template<class Node, class Message>
inline void output_semantic_error(std::shared_ptr<Node> const& node, Message const& msg, std::ostream &ost = std::cerr)
{
    ost << "Semantic error at line:" << node->line << ", col:" << node->col << '\n' << msg << std::endl;
}

// Dynamic resources to use actually
namespace scope {

using dachs::helper::make;

} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

using dachs::helper::variant::apply_lambda;

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
        output_semantic_error(node1, boost::format("Symbol '%1%' is redefined.\nPrevious definition is at line:%2%, col:%3%") % name % node2->line % node2->col);
    }

    template<class Symbol>
    bool define_symbol(std::vector<Symbol> &container, Symbol const& symbol)
    {
        static_assert(std::is_base_of<symbol_node::basic_symbol, typename Symbol::element_type>::value, "define_symbol(): Not a symbol");
        if (auto maybe_duplication = helper::find_if(container, [&symbol](auto const& s){ return *symbol == *s; })) {
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
        return apply_lambda(
                [&name](auto const& s)
                    -> boost::optional<scope::func_scope>
                {
                    return s.lock()->resolve_func(name);
                }, enclosing_scope);
    }

    virtual boost::optional<scope::class_scope> resolve_class(std::string const& name) const
    {
        return apply_lambda(
                [&name](auto const& s)
                    -> boost::optional<scope::class_scope>
                {
                    return s.lock()->resolve_class(name);
                }, enclosing_scope);
    }

    virtual boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const
    {
        return apply_lambda(
                [&name](auto const& s)
                    -> boost::optional<symbol::var_symbol>
                {
                    return s.lock()->resolve_var(name);
                }, enclosing_scope);
    }

    virtual boost::optional<symbol::template_type_symbol> resolve_template_type(std::string const& name) const
    {
        return apply_lambda(
                [&name](auto const& s)
                    -> boost::optional<symbol::template_type_symbol>
                {
                    return s.lock()->resolve_template_type(name);
                }, enclosing_scope);
    }
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
        // TODO: Consider overloaded functions
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
        return target_var ?
                *target_var :
                apply_lambda([&name](auto const& s){ return s.lock()->resolve_var(name); }, enclosing_scope);
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
        return target_var ?
                *target_var :
                apply_lambda([&name](auto const& s)
                        {
                            return s.lock()->resolve_var(name);
                        }, enclosing_scope);
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

struct var_symbol_resolver
    : boost::static_visitor<boost::optional<symbol::var_symbol>> {
    std::string const& name;

    explicit var_symbol_resolver(std::string const& n) noexcept
        : name{n}
    {}

    template<class T>
    result_type operator()(std::shared_ptr<T> const& scope) const noexcept
    {
        return scope->resolve_var(name);
    }
};

struct class_resolver
    : boost::static_visitor<boost::optional<class_scope>> {
    std::string const& name;

    explicit class_resolver(std::string const& n) noexcept
        : name{n}
    {}

    template<class T>
    result_type operator()(std::shared_ptr<T> const& scope) const noexcept
    {
        return scope->resolve_class(name);
    }
};

} // namespace scope

} // namespace dachs

#endif    // DACHS_SCOPE_HPP_INCLUDED
