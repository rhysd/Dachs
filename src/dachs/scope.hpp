#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <memory>
#include <vector>
#include <boost/variant/variant.hpp>

#include "dachs/ast.hpp"
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

#define DACHS_DEFINE_SCOPE(s) \
   using s = std::shared_ptr<scope_node::s>; \
   using weak_##s = std::weak_ptr<scope_node::s>
DACHS_DEFINE_SCOPE(global_scope);
DACHS_DEFINE_SCOPE(local_scope);
DACHS_DEFINE_SCOPE(func_scope);
DACHS_DEFINE_SCOPE(class_scope);
#undef DACHS_DEFINE_SCOPE

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

using parent_scope_type
        = boost::variant< weak_global_scope
                        , weak_local_scope
                        , weak_func_scope
                        , weak_class_scope
                    >;
} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

struct basic_scope {
    scope::parent_scope_type parent_scope;

    explicit basic_scope(scope::parent_scope_type const& parent)
        : parent_scope(parent)
    {}

    explicit basic_scope(scope::any_scope const& parent)
        : parent_scope(parent)
    {}

    virtual ~basic_scope()
    {}
};

struct local_scope final : public basic_scope {
    std::vector<scope::local_scope> children;
    std::vector<symbol::var_symbol> var_symbols;
};

struct func_scope final : public basic_scope {
    symbol::func_symbol symbol;
    scope::local_scope body;
    std::vector<symbol::var_symbol> params;
    template<class P>
    explicit func_scope(P const& p, std::string const& s)
        : basic_scope(p), symbol(symbol::make<symbol::func_symbol>(s))
    {}
};

struct global_scope final : public basic_scope {
    std::vector<scope::func_scope> func_scopes;
    std::vector<symbol::builtin_type_symbol> const builtin_type_symbols;
    std::vector<symbol::var_symbol> var_symbols;

    template<class P>
    explicit global_scope(P const& p)
        : basic_scope(p)
        , builtin_type_symbols({
                symbol::make<symbol::builtin_type_symbol>("int"),
                symbol::make<symbol::builtin_type_symbol>("float"),
                symbol::make<symbol::builtin_type_symbol>("fhar"),
                symbol::make<symbol::builtin_type_symbol>("ftring"),
                symbol::make<symbol::builtin_type_symbol>("fymbol"),
            // {"dict"}
            // {"array"}
            // {"tuple"}
        })
    {}

    void define_function(scope::func_scope const& new_func)
    {
        func_scopes.push_back(new_func);
    }

    void define_global_variable(symbol::var_symbol const& v)
    {
        var_symbols.push_back(v);
    }
};

struct class_scope final : public basic_scope {
    symbol::class_symbol symbol;
    std::vector<scope::func_scope> member_func_scopes;
    std::vector<symbol::member_var_symbol> member_var_symbols;
    std::vector<scope::class_scope> inherited_class_scopes;

    template<class P>
    explicit class_scope(P const& p, std::string const& name)
        : basic_scope(p)
        , symbol(symbol::make<symbol::class_symbol>(name))
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

namespace scope {

struct scope_tree {
    scope::global_scope root;
};

scope_tree make_scope_tree(ast::ast &ast);

std::string dump_scope_tree(scope_tree const& tree);

} // namespace scope
} // namespace dachs

#endif    // DACHS_SCOPE_HPP_INCLUDED
