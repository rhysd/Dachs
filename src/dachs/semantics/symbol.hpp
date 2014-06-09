#if !defined DACHS_SYMBOL_HPP_INCLUDED
#define      DACHS_SYMBOL_HPP_INCLUDED

#include <type_traits>
#include <memory>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace symbol_node {

// TODO:
// Any symbol has type (except for modules or namespaces?)

struct basic_symbol {
    std::string name;
    type::type type;
    ast::node::any_node ast_node;
    bool is_builtin;

    explicit basic_symbol(std::string const& s, bool const is_builtin = false) noexcept
        : name(s), type{}, ast_node{}, is_builtin(is_builtin)
    {}

    template<class Type>
    basic_symbol(std::string const& s, Type const& t, bool const is_builtin = false) noexcept
        : name(s), type{t}, ast_node{}, is_builtin(is_builtin)
    {}

    template<class Node>
    basic_symbol(Node const& node, std::string const& s, bool const is_builtin = false) noexcept
        : name(s), type{}, ast_node{node}, is_builtin(is_builtin)
    {}

    std::string to_string() const noexcept
    {
        return '<' + name + '>';
    }

    virtual ~basic_symbol() noexcept
    {}
};

struct var_symbol final : public basic_symbol {
    bool immutable;

    template<class Node>
    var_symbol(Node const& node, std::string const& s, bool const immutable = true, bool const is_builtin = false) noexcept
        : basic_symbol(node, s, is_builtin), immutable(immutable)
    {}
};

struct member_var_symbol final : public basic_symbol {
    using basic_symbol::basic_symbol;

    scope::class_scope its_class;

    member_var_symbol(std::string const& name, scope::class_scope c, bool const is_builtin = false) noexcept
        : basic_symbol(name, is_builtin), its_class(c)
    {}
};

} // namespace symbol_node

namespace symbol {

#define DACHS_DEFINE_SYMBOL(n) \
   using n = std::shared_ptr<symbol_node::n>; \
   using weak_##n = std::weak_ptr<symbol_node::n>
DACHS_DEFINE_SYMBOL(var_symbol);
DACHS_DEFINE_SYMBOL(member_var_symbol);
#undef DACHS_DEFINE_SYMBOL

using dachs::helper::make;

template<class T>
struct is_symbol_node : std::is_base_of<dachs::symbol_node::basic_symbol, T>{};

} // namespace symbol

// operator== for any symbols
template<class T>
inline
typename std::enable_if<
    symbol::is_symbol_node<T>::value
    , bool
>::type
operator==(T const& l, T const& r) noexcept
{
    return l.name == r.name;
}

template<class T, class U>
inline
typename std::enable_if<
    symbol::is_symbol_node<T>::value
    && symbol::is_symbol_node<U>::value
    && !std::is_same<T, U>::value
    , bool
>::type
operator==(T const&, U const&) noexcept
{
    return false;
}

} // namespace dachs
#endif    // DACHS_SYMBOL_HPP_INCLUDED
