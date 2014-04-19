#if !defined DACHS_SYMBOL_HPP_INCLUDED
#define      DACHS_SYMBOL_HPP_INCLUDED

namespace dachs {
namespace symbol {

struct basic_symbol {
    virtual ~basic_symbol()
    {}
};

struct var_symbol final : public basic_symbol {};

struct func_symbol final : public basic_symbol {};

struct member_func_symbol final : public basic_symbol {};

struct class_symbol final : public basic_symbol {};

} // namespace symbol
} // namespace dachs

#endif    // DACHS_SYMBOL_HPP_INCLUDED
