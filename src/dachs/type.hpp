#if !defined DACHS_TYPE_HPP_INCLUDED
#define      DACHS_TYPE_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "scope_fwd.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace type_node {
struct builtin_type;
struct class_type;
struct tuple_type;
struct func_type;
struct proc_type;
struct dict_type;
struct array_type;
struct qualified_type;
}

namespace type {
#define DACHS_DEFINE_TYPE(n) \
   using n = std::shared_ptr<type_node::n>; \
   using weak_##n = std::weak_ptr<type_node::n>
DACHS_DEFINE_TYPE(builtin_type);
DACHS_DEFINE_TYPE(class_type);
DACHS_DEFINE_TYPE(tuple_type);
DACHS_DEFINE_TYPE(func_type);
DACHS_DEFINE_TYPE(proc_type);
DACHS_DEFINE_TYPE(dict_type);
DACHS_DEFINE_TYPE(array_type);
DACHS_DEFINE_TYPE(qualified_type);
#undef DACHS_DEFINE_TYPE

using any_type
    = boost::variant< builtin_type
                    , class_type
                    , tuple_type
                    , func_type
                    , proc_type
                    , dict_type
                    , array_type
                    , qualified_type
                >;

using type = any_type ; // For external use

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

using dachs::helper::make;

} // namespace type

namespace type_node {

using boost::algorithm::join;
using boost::adaptors::transformed;

namespace detail {
struct to_string : public boost::static_visitor<std::string> {
    template<class Type>
    std::string operator()(Type const& t) const noexcept
    {
        return t->to_string();
    }
};
} // namespace detail

struct basic_type {
    virtual std::string to_string() const noexcept = 0;
    virtual ~basic_type() noexcept
    {}
};

struct named_type : public basic_type {
    std::string name;

    explicit named_type(std::string const& name) noexcept
        : name{name}
    {}

    explicit named_type(std::string && name) noexcept
        : name{std::forward<std::string>(name)}
    {}

    virtual ~named_type() noexcept
    {}
};

struct builtin_type final : public named_type {

    using named_type::named_type;

    std::string to_string() const noexcept override
    {
        return name;
    }
};

// This class may not be needed because class from class template is instanciated at the point on resolving a symbol of class templates
struct class_type final : public named_type {
    std::vector<type::any_type> holder_types;
    scope::class_scope symbol;

    class_type(std::string const& n, scope::class_scope const& s) noexcept
        : named_type(n), symbol(s)
    {}

    std::string to_string() const noexcept override
    {
        return name + '(' +
            join(holder_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct tuple_type final : public basic_type {
    std::vector<type::any_type> element_types;

    tuple_type() = default;

    template<class Vec>
    explicit tuple_type(Vec && v) noexcept
        : element_types{std::forward<decltype(element_types)>(v)}
    {}

    std::string to_string() const noexcept override
    {
        return '(' +
            join(element_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct func_type final : public basic_type {
    std::vector<type::any_type> param_types;
    type::any_type return_type;

    func_type() = default;

    template<class Vec, class Ret>
    func_type(Vec && p, Ret const& r) noexcept
        : param_types{std::forward<decltype(param_types)>(p)}, return_type(r)
    {}

    std::string to_string() const noexcept override
    {
        return "func (" +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ") : "
            + boost::apply_visitor(detail::to_string{}, return_type);
    }
};

struct proc_type final : public basic_type {
    std::vector<type::any_type> param_types;

    proc_type() = default;

    template<class Vec>
    explicit proc_type(Vec && p) noexcept
        : param_types{std::forward<decltype(param_types)>(p)}
    {}

    std::string to_string() const noexcept override
    {
        return "proc (" +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct dict_type final : public basic_type {
    type::any_type key_type, value_type;

    dict_type() = default;

    template<class Key, class Value>
    dict_type(Key const& k, Value const& v) noexcept
        : key_type{k}, value_type{v}
    {}

    std::string to_string() const noexcept override
    {
        detail::to_string v;
        return '{'
            + boost::apply_visitor(v, key_type) + " => "
            + boost::apply_visitor(v, value_type)
            + '}';
    }
};

struct array_type final : public basic_type {
    type::any_type element_type;

    array_type() = default;

    template<class Elem>
    explicit array_type(Elem const& e) noexcept
        : element_type(e)
    {}

    std::string to_string() const noexcept override
    {
        return '{'
            + boost::apply_visitor(detail::to_string{}, element_type)
            + '}';
    }
};

struct qualified_type final : public basic_type {
    type::qualifier qualifier;
    type::any_type contained_type;

    template<class Contained>
    qualified_type(type::qualifier const q, Contained const& c) noexcept
        : qualifier{q}, contained_type{c}
    {}

    std::string to_string() const noexcept override
    {
        auto const contained_type_name =  boost::apply_visitor(detail::to_string{}, contained_type);
        switch (qualifier) {
        case type::qualifier::maybe:
            return contained_type_name + '?';
        default:
            assert(false);
        }
    }
};

} // namespace type_node

} // namespace dachs

#endif    // DACHS_TYPE_HPP_INCLUDED
