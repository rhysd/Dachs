#if !defined DACHS_SEMANTICS_TYPE_HPP_INCLUDED
#define      DACHS_SEMANTICS_TYPE_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/variant/apply_visitor.hpp>
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
#include "dachs/helper/probable.hpp"
#include "dachs/fatal.hpp"

namespace dachs {

namespace type_node {
struct basic_type;
struct builtin_type;
struct class_type;
struct tuple_type;
struct func_type;
struct generic_func_type;
struct array_type;
struct pointer_type;
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
DACHS_DEFINE_TYPE(generic_func_type);
DACHS_DEFINE_TYPE(array_type);
DACHS_DEFINE_TYPE(pointer_type);
DACHS_DEFINE_TYPE(qualified_type);
DACHS_DEFINE_TYPE(template_type);
#undef DACHS_DEFINE_TYPE

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

using dachs::helper::make;

struct no_opt_t {};
extern no_opt_t no_opt;
boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept;
builtin_type get_builtin_type(char const* const name, no_opt_t) noexcept;
tuple_type const& get_unit_type() noexcept;

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

using ::dachs::helper::enable_if;
using ::dachs::helper::variant::apply_lambda;

class any_type {
    using value_type
        = boost::variant<
                      builtin_type
                    , class_type
                    , tuple_type
                    , func_type
                    , generic_func_type
                    , array_type
                    , pointer_type
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
    value_type &raw_value() noexcept
    {
        return value;
    }

    value_type const& raw_value() const noexcept
    {
        return value;
    }

    bool is_template() const noexcept;

    bool is_class_template() const noexcept;

    bool is_builtin() const noexcept
    {
        return helper::variant::has<builtin_type>(value);
    }

    template<class String>
    bool is_builtin(String const& name) const noexcept;

    bool is_unit() const noexcept;

    bool is_array_class() const noexcept;
    bool is_string_class() const noexcept;
    bool is_aggregate() const noexcept;

    boost::optional<pointer_type const&> get_array_underlying_type() const;
    boost::optional<pointer_type const&> get_string_underlying_type() const;

    // Note:
    // Visitor && is not available because boost::apply_visitor
    // can't take rvalue arguments.

    template<class Visitor>
    typename Visitor::result_type apply_visitor(Visitor &visitor)
    {
        return boost::apply_visitor(visitor, value);
    }

    template<class Visitor>
    typename Visitor::result_type apply_visitor(Visitor &visitor) const
    {
        return boost::apply_visitor(visitor, value);
    }

    template<class Lambda>
    auto apply_lambda(Lambda const& lambda) const
        -> decltype(apply_lambda(lambda, value))
    {
        return helper::variant::apply_lambda(lambda, value);
    }

    template<class Type>
    bool is_instantiated_from(Type const& from) const;
    bool is_instantiated_from(any_type const& from) const;

    bool is_default_constructible() const noexcept;

    template<class T>
    friend bool is_a(any_type const&);

    template<class T>
    friend boost::optional<T const&> get(any_type const&);
};

template<class T>
inline bool is_a(any_type const& t)
{
    return helper::variant::has<T>(t.value);
}

// Note:
// When the argument of has() is not any_type
template<class T, class U>
inline constexpr bool is_a(U const&)
{
    return std::is_same<T, U>::value;
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
    virtual bool is_default_constructible() const noexcept = 0;
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

    bool is_default_constructible() const noexcept override
    {
        return name != "symbol";
    }
};

// This class may not be needed because class from class template is instanciated at the point on resolving a symbol of class templates
struct class_type final : public named_type {
    scope::weak_class_scope ref;
    std::vector<type::any_type> param_types;

    explicit class_type(scope::class_scope const& s) noexcept;
    class_type(class_type const&);

    template<class Types>
    class_type(scope::class_scope const& s, Types const& types) noexcept
        : class_type(s)
    {
        for (auto const& t : types) {
            param_types.push_back(t);
        }
    }

    std::string stringize_param_types() const;
    std::string to_string() const noexcept override;

    bool is_template() const;

    bool operator==(class_type const& rhs) const noexcept;

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

    bool is_default_constructible() const noexcept override;

    bool is_instantiated_from(type::class_type const&) const;

    boost::optional<type::pointer_type const&> get_array_underlying_type() const;
    boost::optional<type::pointer_type const&> get_string_underlying_type() const;
};

struct tuple_type final : public basic_type {
    std::vector<type::any_type> element_types;

    tuple_type() = default;

    template<class Range>
    explicit tuple_type(Range const& r) noexcept
        : element_types{r.begin(), r.end()}
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

    bool is_default_constructible() const noexcept override
    {
        return true;
    }
};

struct func_type final : public basic_type {
    std::vector<type::any_type> param_types;
    type::any_type return_type;
    ast::symbol::func_kind kind = ast::symbol::func_kind::func;

    func_type() = default;

    func_type(decltype(param_types) && p, type::any_type && r, decltype(kind) const k = ast::symbol::func_kind::func) noexcept
        : param_types(std::forward<decltype(p)>(p)), return_type(std::forward<type::any_type>(r)), kind(k)
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

    bool is_default_constructible() const noexcept override
    {
        return false;
    }
};

struct generic_func_type : public basic_type {
    boost::optional<scope::weak_func_scope> ref = boost::none;

    template<class FuncScope>
    explicit generic_func_type(FuncScope const& r)
        : ref(r)
    {}

    generic_func_type() = default;

    bool operator==(generic_func_type const&) const noexcept;

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "generic_func_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "generic_func_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }

    std::string to_string() const noexcept override;

    bool is_default_constructible() const noexcept override
    {
        return false;
    }
};

struct array_type final : public basic_type {
    type::any_type element_type;
    boost::optional<size_t> size = boost::none;

    template<class Elem>
    explicit array_type(Elem const& e) noexcept
        : element_type(e)
    {}

    template<class Elem>
    array_type(Elem const& e, size_t const s) noexcept
        : element_type(e), size(s)
    {}

    std::string to_string() const noexcept override
    {
        return "static_array(" + element_type.to_string()
            + (size ? ")(" + std::to_string(*size) + ')' : ")");
    }

    bool operator==(array_type const& rhs) const noexcept
    {
        return element_type == rhs.element_type && size == rhs.size;
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

    bool is_default_constructible() const noexcept override
    {
        return true;
    }
};

struct pointer_type final : public basic_type {
    type::any_type pointee_type;

    template<class Elem>
    explicit pointer_type(Elem && e) noexcept
        : pointee_type(std::forward<Elem>(e))
    {}

    std::string to_string() const noexcept override
    {
        return "pointer(" + pointee_type.to_string() + ")";
    }

    bool operator==(pointer_type const& rhs) const noexcept
    {
        return pointee_type == rhs.pointee_type;
    }

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "pointer_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "pointer_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }

    bool is_default_constructible() const noexcept override
    {
        return false;
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

    bool is_default_constructible() const noexcept override
    {
        // TODO: TEMPORARY
        return contained_type.is_default_constructible();
    }
};

struct template_type final : public basic_type {
    ast::node::any_node ast_node;

    template<class Node>
    explicit template_type(Node const& node) noexcept
        : ast_node(node)
    {}

    boost::optional<ast::node::parameter> get_ast_node_as_parameter() const noexcept;
    boost::optional<ast::node::variable_decl> get_ast_node_as_var_decl() const noexcept;
    std::string to_string() const noexcept override;

    template<class T>
    bool operator==(T const&) const noexcept
    {
        static_assert(is_type<T>::value, "template_type::operator==(): rhs is not a type.");
        return false;
    }

    template<class T>
    bool operator!=(T const& rhs) const noexcept
    {
        static_assert(is_type<T>::value, "template_type::operator!=(): rhs is not a type.");
        return !(*this == rhs);
    }

    bool is_default_constructible() const noexcept override
    {
        return false;
    }
};

} // namespace type_node

namespace type {

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

template<class Variant>
inline any_type type_of(Variant const& v) noexcept
{
    return apply_lambda([](auto const& n){ return n->type; }, v);
}

ast::node::any_type to_ast(any_type const&, ast::location_type &&) noexcept;
ast::node::any_type to_ast(any_type const&, ast::location_type const&) noexcept;
bool is_instantiated_from(class_type const& instantiated_class, class_type const& template_class);
bool is_instantiated_from(array_type const& instantiated_array, array_type const& template_array);
bool is_instantiated_from(pointer_type const& instantiated_ptr, pointer_type const& template_ptr);

template<class String>
bool any_type::is_builtin(String const& name) const noexcept
{
    auto const t = helper::variant::get_as<builtin_type>(value);
    if (!t) {
        return false;
    }

    return (*t)->name == name;
}

template<class Type>
bool any_type::is_instantiated_from(Type const& from) const
{
    if (auto const t = helper::variant::get_as<Type>(value)) {
        return ::dachs::type::is_instantiated_from(*t, from);
    } else {
        return false;
    }
}

bool fuzzy_match(any_type const& lhs, any_type const& rhs);

} // namespace type

} // namespace dachs

#endif    // DACHS_SEMANTICS_TYPE_HPP_INCLUDED
