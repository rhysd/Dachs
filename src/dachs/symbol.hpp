#if !defined DACHS_SYMBOL_HPP_INCLUDED
#define      DACHS_SYMBOL_HPP_INCLUDED

#include <memory>

#include "type.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace symbol_node {

// TODO:
// Any symbol has type (except for modules or namespaces?)

struct basic_symbol {
    std::string name;
    // Type type;

    explicit basic_symbol(std::string const& s) noexcept
        : name(s)
    {}

    std::string to_string() const noexcept
    {
        return '<' + name + '>';
    }

    virtual ~basic_symbol() noexcept
    {}
};

struct var_symbol final : public basic_symbol {
    explicit var_symbol(std::string const& s) noexcept
        : basic_symbol(s)
    {}
};

struct member_func_symbol final : public basic_symbol {
    explicit member_func_symbol(std::string const& s) noexcept
        : basic_symbol(s)
    {}
};

struct member_var_symbol final : public basic_symbol {
    explicit member_var_symbol(std::string const& s) noexcept
        : basic_symbol(s)
    {}
};

struct builtin_type_symbol final : public basic_symbol {
    explicit builtin_type_symbol(std::string const& s) noexcept
        : basic_symbol(s)
    {}
};

} // namespace symbol_node

namespace symbol {

#define DACHS_DEFINE_SYMBOL(n) \
   using n = std::shared_ptr<symbol_node::n>; \
   using weak_##n = std::weak_ptr<symbol_node::n>
DACHS_DEFINE_SYMBOL(var_symbol);
DACHS_DEFINE_SYMBOL(member_func_symbol);
DACHS_DEFINE_SYMBOL(member_var_symbol);
DACHS_DEFINE_SYMBOL(builtin_type_symbol);
#undef DACHS_DEFINE_SYMBOL

using dachs::helper::make;

} // namespace symbol
} // namespace dachs

#endif    // DACHS_SYMBOL_HPP_INCLUDED
