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
#include <boost/mpl/vector.hpp>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/fatal.hpp"

namespace dachs {

namespace type_node {
struct basic_type;
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
struct template_type;
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
DACHS_DEFINE_TYPE(template_type);
#undef DACHS_DEFINE_TYPE

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

using dachs::helper::make;
using dachs::helper::variant::apply_lambda;

struct no_opt_t {};
extern no_opt_t no_opt;
boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept;
builtin_type get_builtin_type(char const* const name, no_opt_t) noexcept;

namespace traits {

template<class T>
struct is_type
    : std::is_base_of<
        type_node::basic_type
      , typename std::remove_reference<
            typename std::remove_cv<T>::type
        >::type
    > {};

} // namespace traits

using dachs::helper::enable_if;

class any_type {
    using value_type
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
                    , template_type
                >;

    template<class T, class Head, class... Tail>
    struct is_held_impl2 : is_held_impl2<T, Tail...>::type
    {};

    template<class T, class... Tail>
    struct is_held_impl2<T, T, Tail...> : std::true_type
    {};

    template<class T, class U>
    struct is_held_impl2<T, U> : std::is_same<T, U>
    {};

    template<class T, class Variant>
    struct is_held_impl;

    template<class T, class... Args>
    struct is_held_impl<T, boost::variant<Args...>> : is_held_impl2<T, Args...>
    {};

    template<class T>
    struct is_held : is_held_impl<T, value_type>
    {};

    value_type value;

public:

    any_type() = default;
    any_type(any_type const& t) = default;
    any_type(any_type && t) = default;
    any_type &operator=(any_type const& t) = default;

    template<class T, class = enable_if<is_held<T>::value>>
    any_type(T const& v)
        : value(v)
    {}

    template<class T, class = enable_if<is_held<T>::value>>
    any_type(T && v)
        : value(std::forward<T>(v))
    {}

    template<class T, class = enable_if<is_held<T>::value>>
    any_type &operator=(T const& rhs)
    {
        value = rhs;
        return *this;
    }

    template<class T, class = enable_if<is_held<T>::value>>
    any_type &operator=(any_type && rhs)
    {
        std::swap(value, rhs.value);
        return *this;
    }

    bool operator==(any_type const& rhs) const noexcept;

    bool operator!=(any_type const& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    bool empty() const noexcept
    {
        return helper::variant::apply_lambda([](auto const& t){ return !bool(t); }, value);
    }

    operator bool() const noexcept
    {
        return !empty();
    }

    std::string to_string() const noexcept
    {
        return helper::variant::apply_lambda([](auto const& t) -> std::string { return t ? t->to_string() : "UNKNOWN"; }, value);
    }

    // Note: This may ruin the private data member. Be careful.
    value_type &raw_type() noexcept
    {
        return value;
    }

    value_type const& raw_type() const noexcept
    {
        return value;
    }

    bool is_template() const noexcept
    {
        return helper::variant::has<type::template_type>(value);
    }

    template<class T>
    friend bool has(any_type const&);

    template<class T>
    friend boost::optional<T const&> get(any_type const&);
};

template<class T>
inline bool has(any_type const& t)
{
    return helper::variant::has<T>(t.value);
}

template<class T>
inline boost::optional<T const&> get(any_type const& t)
{
    return helper::variant::get_as<T>(t.value);
}

} // namespace type

namespace type_node {

using boost::algorithm::join;
using boost::adaptors::transformed;
using dachs::helper::enable_if;
using type::traits::is_type;

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

    // TODO: Restrict T in type nodes
    bool operator==(builtin_type const& rhs) const noexcept
    {
        return name == rhs.name;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "builtin_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "builtin_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
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
                            return t.to_string();
                        }), ",")
                + ')';
        }
    }

    bool operator==(class_type const& rhs) const noexcept
    {
        return name == rhs.name && boost::equal(holder_types, rhs.holder_types);
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "class_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "class_type::operator!=(): rhs is not a type.");
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
                        return t.to_string();
                    }), ",")
            + ')';
    }

    bool operator==(tuple_type const& rhs) const noexcept
    {
        return boost::equal(element_types, rhs.element_types);
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "tuple_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "tuple_type::operator!=(): rhs is not a type.");
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
                        return t.to_string();
                    }), ",")
            + ") : "
            + return_type.to_string();
    }

    bool operator==(func_type const& rhs) const noexcept
    {
        return boost::equal(param_types, rhs.param_types)
            && return_type == rhs.return_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "func_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "func_type::operator!=(): rhs is not a type.");
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
                        return t.to_string();
                    }), ",")
            + ')';
    }

    bool operator==(proc_type const& rhs) const noexcept
    {
        return boost::equal(param_types, rhs.param_types);
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "proc_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "proc_type::operator!=(): rhs is not a type.");
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

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "func_ref_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "func_ref_type::operator!=(): rhs is not a type.");
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
            + key_type.to_string() + " => "
            + value_type.to_string()
            + '}';
    }

    bool operator==(dict_type const& rhs) const noexcept
    {
        return key_type == rhs.key_type
            && value_type == rhs.value_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "dict_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "dict_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }
};

struct range_type final : public basic_type {
    type::any_type from_type, to_type;
    // TODO: bool is_inclusive

    range_type() = default;

    template<class From, class To>
    range_type(From const& f, To const& t) noexcept
        : from_type{f}, to_type{t}
    {}

    std::string to_string() const noexcept override
    {
        return from_type.to_string()
             + ".."
             + to_type.to_string();
    }

    bool operator==(range_type const& rhs) const noexcept
    {
        return from_type == rhs.from_type
            && to_type == rhs.to_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "range_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "range_type::operator!=(): rhs is not a type.");
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
        return '['
            + element_type.to_string()
            + ']';
    }

    bool operator==(array_type const& rhs) const noexcept
    {
        return element_type == rhs.element_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "array_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "array_type::operator!=(): rhs is not a type.");
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
        auto const contained_type_name =  contained_type.to_string();
        switch (qualifier) {
        case type::qualifier::maybe:
            return contained_type_name + '?';
        default:
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    bool operator==(qualified_type const& rhs) const noexcept
    {
        return contained_type == rhs.contained_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "qualified_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "qualified_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }
};

struct template_type final : public basic_type {
    ast::node::any_node ast_node;

    template<class Node>
    explicit template_type(Node const& node) noexcept
        : ast_node(node)
    {}

    boost::optional<ast::node::parameter> get_ast_node_as_parameter() const noexcept;
    std::string to_string() const noexcept override;

    bool operator==(template_type const& rhs)
    {
        auto const left_ast_as_param = get_ast_node_as_parameter();
        auto const right_ast_as_param = rhs.get_ast_node_as_parameter();

        if (left_ast_as_param && right_ast_as_param) {
            // Compare two shared_ptr.  Return true when both point the same node.
            return *left_ast_as_param == *right_ast_as_param;
        }

        // TODO: Add more possible nodes: instance variables
        // ...

        return false;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "qualified_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "qualified_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }
};

} // namespace type_node

namespace type {

template<class Lambda, class... Args>
inline auto apply_lambda_(Lambda const& l, Args const&... args)
{
    return apply_lambda(l, args.raw_type()...);
}

template<class Lambda, class... Args>
inline auto apply_lambda_(Lambda &l, Args &... args)
{
    return apply_lambda(l, args.raw_type()...);
}

using type = any_type ; // For external use

template<class T, class = typename std::enable_if<traits::is_type<T>::value>::type>
inline std::string to_string(T const& type) noexcept
{
    return type->to_string();
}

inline std::string to_string(any_type const& type) noexcept
{
    return type.to_string();
}

} // namespace type

} // namespace dachs

#endif    // DACHS_TYPE_HPP_INCLUDED
