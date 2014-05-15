#if !defined DACHS_TYPE_HPP_INCLUDED
#define      DACHS_TYPE_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/equal.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/optional.hpp>

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
struct func_ref_type;
struct dict_type;
struct array_type;
struct range_type;
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
DACHS_DEFINE_TYPE(func_ref_type);
DACHS_DEFINE_TYPE(dict_type);
DACHS_DEFINE_TYPE(array_type);
DACHS_DEFINE_TYPE(range_type);
DACHS_DEFINE_TYPE(qualified_type);
#undef DACHS_DEFINE_TYPE

using any_type
    = boost::variant< builtin_type
                    , class_type
                    , tuple_type
                    , func_type
                    , proc_type
                    , func_ref_type
                    , dict_type
                    , array_type
                    , range_type
                    , qualified_type
                >;

using type = any_type ; // For external use

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

using dachs::helper::make;

boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept;

bool compare_types(any_type const& lhs, any_type const& rhs) noexcept;

} // namespace type

namespace type_node {

using boost::algorithm::join;
using boost::adaptors::transformed;

namespace detail {

inline std::string to_string(type::any_type const& t) noexcept
{
    return helper::variant::apply_lambda([](auto const& t) -> std::string { return t->to_string(); }, t);
}

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

    bool operator==(named_type const& rhs) const noexcept
    {
        return name == rhs.name;
    }

    bool operator!=(named_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
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
        if (holder_types.empty()) {
            return name;
        } else {
            return name + '(' +
                join(holder_types | transformed([](auto const& t){
                            return detail::to_string(t);
                        }), ",")
                + ')';
        }
    }

    bool operator==(class_type const& rhs) const noexcept
    {
        return name == rhs.name && boost::equal(holder_types, rhs.holder_types, type::compare_types);
    }

    bool operator!=(class_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct tuple_type final : public basic_type {
    std::vector<type::any_type> element_types;

    tuple_type() = default;

    explicit tuple_type(decltype(element_types) const& v) noexcept
        : element_types(v)
    {}

    std::string to_string() const noexcept override
    {
        return '(' +
            join(element_types | transformed([](auto const& t){
                        return detail::to_string(t);
                    }), ",")
            + ')';
    }

    bool operator==(tuple_type const& rhs) const noexcept
    {
        return boost::equal(element_types, rhs.element_types, type::compare_types);
    }

    bool operator!=(tuple_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct func_type final : public basic_type {
    std::vector<type::any_type> param_types;
    type::any_type return_type;

    func_type() = default;

    func_type(decltype(param_types) && p, type::any_type && r) noexcept
        : param_types(std::forward<decltype(p)>(p)), return_type(std::forward<type::any_type>(r))
    {}

    std::string to_string() const noexcept override
    {
        return "func (" +
            join(param_types | transformed([](auto const& t){
                        return detail::to_string(t);
                    }), ",")
            + ") : "
            + detail::to_string(return_type);
    }

    bool operator==(func_type const& rhs) const noexcept
    {
        return boost::equal(param_types, rhs.param_types, type::compare_types)
            && type::compare_types(return_type, rhs.return_type);
    }

    bool operator!=(func_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct proc_type final : public basic_type {
    std::vector<type::any_type> param_types;

    proc_type() = default;

    explicit proc_type(decltype(param_types) && p) noexcept
        : param_types(std::forward<decltype(p)>(p))
    {}

    std::string to_string() const noexcept override
    {
        return "proc (" +
            join(param_types | transformed([](auto const& t){
                        return detail::to_string(t);
                    }), ",")
            + ')';
    }

    bool operator==(proc_type const& rhs) const noexcept
    {
        return boost::equal(param_types, rhs.param_types, type::compare_types);
    }

    bool operator!=(proc_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct func_ref_type : public basic_type {
    boost::optional<scope::weak_func_scope> ref = boost::none;

    template<class FuncScope>
    explicit func_ref_type(FuncScope const& r)
        : ref(r)
    {}

    func_ref_type() = default;

    bool operator==(func_ref_type const&) const noexcept;

    bool operator!=(func_ref_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    std::string to_string() const noexcept override;
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
        return '{'
            + detail::to_string(key_type) + " => "
            + detail::to_string(value_type)
            + '}';
    }

    bool operator==(dict_type const& rhs) const noexcept
    {
        return type::compare_types(key_type, rhs.key_type)
            && type::compare_types(value_type, rhs.value_type);
    }

    bool operator!=(dict_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

struct range_type final : public basic_type {
    type::any_type from_type, to_type;

    range_type() = default;

    template<class From, class To>
    range_type(From const& f, To const& t) noexcept
        : from_type{f}, to_type{t}
    {}

    std::string to_string() const noexcept override
    {
        return detail::to_string(from_type)
             + ".."
             + detail::to_string(to_type);
    }

    bool operator==(range_type const& rhs) const noexcept
    {
        return type::compare_types(from_type, rhs.from_type)
            && type::compare_types(to_type, rhs.to_type);
    }

    bool operator!=(range_type const& rhs) const noexcept
    {
        return !(*this == rhs);
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
            + detail::to_string(element_type)
            + '}';
    }

    bool operator==(array_type const& rhs) const noexcept
    {
        return type::compare_types(element_type, rhs.element_type);
    }

    bool operator!=(array_type const& rhs) const noexcept
    {
        return !(*this == rhs);
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
        auto const contained_type_name =  detail::to_string(contained_type);
        switch (qualifier) {
        case type::qualifier::maybe:
            return contained_type_name + '?';
        default:
            assert(false);
        }
    }

    bool operator==(qualified_type const& rhs) const noexcept
    {
        return type::compare_types(contained_type, rhs.contained_type);
    }

    bool operator!=(qualified_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

} // namespace type_node

namespace type {

namespace detail {

struct type_equal : public boost::static_visitor<bool> {
    template<class T>
    bool operator()(std::shared_ptr<T> const& l, std::shared_ptr<T> const& r) const noexcept
    {
        return *l == *r;
    }

    template<class T, class U>
    bool operator()(std::shared_ptr<T> const&, std::shared_ptr<U> const&) const noexcept
    {
        return false;
    }
};

} // namespace detail

inline bool compare_types(any_type const& lhs, any_type const& rhs) noexcept
{
    return boost::apply_visitor(detail::type_equal{}, lhs, rhs);
}

} // namespace type

} // namespace dachs

#endif    // DACHS_TYPE_HPP_INCLUDED
