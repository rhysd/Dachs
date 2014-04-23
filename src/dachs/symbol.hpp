#if !defined DACHS_SYMBOL_HPP_INCLUDED
#define      DACHS_SYMBOL_HPP_INCLUDED

#include <memory>

namespace dachs {
namespace symbol_node {

// TODO:
// Any symbol has type (except for modules or namespaces?)

struct basic_symbol {
    std::string name;
    // Type type;

    explicit basic_symbol(std::string const& s)
        : name(s)
    {}

    std::string to_string() const
    {
        return '<' + name + '>';
    }

    virtual ~basic_symbol()
    {}
};

struct var_symbol final : public basic_symbol {
    explicit var_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};
struct func_symbol final : public basic_symbol {
    explicit func_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};
struct class_symbol final : public basic_symbol {
    explicit class_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};
struct member_func_symbol final : public basic_symbol {
    explicit member_func_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};
struct member_var_symbol final : public basic_symbol {
    explicit member_var_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};
struct builtin_type_symbol final : public basic_symbol {
    explicit builtin_type_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};

// XXX: This class should be rewritten...
struct template_type_symbol final : public basic_symbol {
    explicit template_type_symbol(std::string const& s)
        : basic_symbol(s)
    {}
};

} // namespace symbol_node

namespace symbol {

#define DACHS_DEFINE_SYMBOL(n) \
   using n = std::shared_ptr<symbol_node::n>; \
   using weak_##n = std::weak_ptr<symbol_node::n>
DACHS_DEFINE_SYMBOL(var_symbol);
DACHS_DEFINE_SYMBOL(func_symbol);
DACHS_DEFINE_SYMBOL(class_symbol);
DACHS_DEFINE_SYMBOL(member_func_symbol);
DACHS_DEFINE_SYMBOL(member_var_symbol);
DACHS_DEFINE_SYMBOL(builtin_type_symbol);
DACHS_DEFINE_SYMBOL(template_type_symbol);
#undef DACHS_DEFINE_SYMBOL

template<class Sym, class... Args>
inline Sym make(Args &&... args)
{
    return std::make_shared<typename Sym::element_type>(std::forward<Args>(args)...);
}
} // namespace symbol
} // namespace dachs

#endif    // DACHS_SYMBOL_HPP_INCLUDED
