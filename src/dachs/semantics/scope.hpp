#if !defined DACHS_SEMANTICS_SCOPE_HPP_INCLUDED
#define      DACHS_SEMANTICS_SCOPE_HPP_INCLUDED

#include <vector>
#include <string>
#include <type_traits>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/format.hpp>

#include "dachs/warning.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast_fwd.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {

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

    template<class Symbol>
    bool define_symbol(std::vector<Symbol> &container, Symbol const& symbol)
    {
        static_assert(std::is_base_of<symbol_node::basic_symbol, typename Symbol::element_type>::value, "define_symbol(): Not a symbol");
        if (auto maybe_duplication = helper::find_if(container, [&symbol](auto const& s){ return *symbol == *s; })) {
            semantics::print_duplication_error(symbol->ast_node.get_shared(), (*maybe_duplication)->ast_node.get_shared(), symbol->name);
            return false;
        }
        // TODO: raise warning when a variable shadows other variables
        container.push_back(symbol);
        return true;
    }

    // TODO resolve member variables and member functions

    virtual boost::optional<scope::func_scope>
    resolve_func( std::string const& name
                , std::vector<type::type> const& args) const
    {
        // TODO:
        // resolve_func() now searches function scopes directly.
        // But it should search variables, check the result is funcref type, then resolve function overloads.
        return apply_lambda(
                [&](auto const& s)
                    -> boost::optional<scope::func_scope>
                {
                    return s.lock()->resolve_func(name, args);
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

    void check_shadowing_variable(symbol::var_symbol new_var) const
    {
        auto const maybe_shadowing_var = apply_lambda(
                [&new_var](auto const& s)
                    -> boost::optional<symbol::var_symbol>
                {
                    return s.lock()->resolve_var(new_var->name);
                }, enclosing_scope);

        if (maybe_shadowing_var) {
            auto const the_node = new_var->ast_node.get_shared();
            auto const prev_node = (*maybe_shadowing_var)->ast_node.get_shared();
            output_warning(the_node, boost::format(
                            "Shadowing variable '%1%'. It shadows a variable at line:%2%, col:%3%"
                        ) % new_var->name % prev_node->line % prev_node->col
                    );
        }
    }
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> functions;
    std::vector<symbol::var_symbol> const_symbols;
    std::vector<scope::class_scope> classes;
    std::weak_ptr<ast::node_type::inu> ast_root;

    template<class RootType>
    global_scope(RootType const& ast_root) noexcept
        : basic_scope(), ast_root(ast_root)
    {}

    // Check function duplication after forward analysis because of overload resolution
    void define_function(scope::func_scope const& new_func) noexcept
    {
        // Do check nothing
        functions.push_back(new_func);
    }

    bool define_variable(symbol::var_symbol const& new_var) noexcept
    {
        return define_symbol(const_symbols, new_var);
    }

    // Note:
    // Do not check duplication because of overloaded functions.  Check for overloaded functions
    // is already done by define_function()
    void define_global_function_constant(symbol::var_symbol const& new_var) noexcept
    {
        const_symbols.push_back(new_var);
    }

    bool define_class(scope::class_scope const& new_class) noexcept
    {
        return define_symbol(classes, new_class);
    }

    boost::optional<scope::func_scope>
    resolve_func( std::string const& name, std::vector<type::type> const& args) const override;

    boost::optional<scope::class_scope> resolve_class(std::string const& name) const override
    {
        return helper::find_if(classes, [&name](auto const& c){ return c->name == name; });
    }

    boost::optional<symbol::var_symbol> resolve_var(std::string const& name) const override
    {
        return helper::find_if(const_symbols, [&name](auto const& v){ return v->name == name; });
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

    bool define_variable(symbol::var_symbol const& new_var) noexcept
    {
        check_shadowing_variable(new_var);
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

struct func_scope final : public basic_scope, public symbol_node::basic_symbol {
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;
    boost::optional<type::type> ret_type;

    template<class Node, class P>
    explicit func_scope(Node const& n, P const& p, std::string const& s, bool const is_builtin = false) noexcept
        : basic_scope(p)
        , basic_symbol(n, s, is_builtin)
    {}

    bool define_param(symbol::var_symbol const& new_var) noexcept
    {
        check_shadowing_variable(new_var);
        return define_symbol(params, new_var);
    }

    bool is_template() const noexcept
    {
        for (auto const& p : params) {
            if (p->type.is_template()) {
                return true;
            }
        }
        return false;
    }

    ast::node::function_definition get_ast_node() const noexcept;

    std::string to_string() const noexcept;

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

    // Compare with rhs considering overloading
    bool operator==(func_scope const& rhs) const noexcept;

    bool operator!=(func_scope const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct class_scope final : public basic_scope, public symbol_node::basic_symbol {
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;

    // std::vector<type> for instanciated types (if this isn't template, it should contains only one element)

    template<class Node, class Parent>
    explicit class_scope(Node const& ast_node, Parent const& p, std::string const& name, bool const is_builtin = false) noexcept
        : basic_scope(p)
        , basic_symbol(ast_node, name, is_builtin)
    {}

    bool define_member_func(scope::func_scope const& new_func) noexcept
    {
        return define_symbol(member_func_scopes, new_func);
    }

    bool define_member_var_symbols(symbol::member_var_symbol const& new_var) noexcept
    {
        return define_symbol(member_var_symbols, new_var);
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

} // namespace scope

} // namespace dachs

#endif    // DACHS_SEMANTICS_SCOPE_HPP_INCLUDED
