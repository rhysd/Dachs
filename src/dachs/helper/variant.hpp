#if !defined DACHS_HELPER_VARIANT_HPP_INCLUDED
#define      DACHS_HELPER_VARIANT_HPP_INCLUDED

#include <utility>
#include <type_traits>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>
#include <boost/optional.hpp>

namespace dachs {
namespace helper {
namespace variant {

namespace detail {

template<class T, class Head, class... Tail>
struct is_included : is_included<T, Tail...>::type
{};

template<class T, class... Tail>
struct is_included<T, T, Tail...> : std::true_type
{};

template<class T, class U>
struct is_included<T, U> : std::is_same<T, U>
{};

} // namespace detail

template<class T, class... Args>
inline boost::optional<T const&> get_as(boost::variant<Args...> const& v) noexcept
{
    static_assert(detail::is_included<T, Args...>::value, "get_as(const&): T is not included in Args.");
    T const* const ptr = boost::get<T>(&v);
    if (ptr) {
        return {*ptr};
    } else {
        return boost::none;
    }
}

template<class T, class... Args>
inline boost::optional<T &> get_as(boost::variant<Args...> &v) noexcept
{
    static_assert(detail::is_included<T, Args...>::value, "get_as(&): T is not included in Args.");
    T *const ptr = boost::get<T>(&v);
    if (ptr) {
        return {*ptr};
    } else {
        return boost::none;
    }
}

template<class T, class... Args>
inline T const& get_assert(boost::variant<Args...> const& v) noexcept
{
    static_assert(detail::is_included<T, Args...>::value, "get_assert(const&): T is not included in Args.");
    T const* const ptr = boost::get<T>(&v);
    assert(ptr);
    return {*ptr};
}

template<class T, class... Args>
inline T &get_assert(boost::variant<Args...> &v) noexcept
{
    static_assert(detail::is_included<T, Args...>::value, "get_assert(&): T is not included in Args.");
    T *const ptr = boost::get<T>(&v);
    assert(ptr);
    return {*ptr};
}

// Copy to avoid dangling reference to the content of variant
template<class T, class... Args>
inline boost::optional<T> copy_as(boost::variant<Args...> const& v) noexcept
{
    static_assert(detail::is_included<T, Args...>::value, "copy_as(const&): T is not included in Args.");
    T const* const ptr = boost::get<T>(&v);
    if (ptr) {
        return {*ptr};
    } else {
        return boost::none;
    }
}

template<class T, class... Args>
inline bool has(boost::variant<Args...> const& v)
{
    return boost::get<T>(&v);
}

namespace detail {

template<class Lambda, class Result>
struct lambda_wrapped_visitor
    : public boost::static_visitor<Result>
    , public Lambda {

    lambda_wrapped_visitor(Lambda const& l) noexcept
        : Lambda(l)
    {}

};
} // namespace detail

template<class Lambda, class Head, class... Tail>
inline auto apply_lambda(Lambda const& l, boost::variant<Head, Tail...> const& variant)
{
    return boost::apply_visitor(detail::lambda_wrapped_visitor<Lambda, decltype(std::declval<Lambda>()(std::declval<Head const&>()))>{l}, variant);
}

template<class Lambda, class Head, class... Tail>
inline auto apply_lambda(Lambda const& l, boost::variant<Head, Tail...> &variant)
{
    return boost::apply_visitor(detail::lambda_wrapped_visitor<Lambda, decltype(std::declval<Lambda>()(std::declval<Head &>()))>{l}, variant);
}

template<class Lambda, class Head, class... Tail, class Head2, class... Tail2>
inline auto apply_lambda(Lambda const& l, boost::variant<Head, Tail...> const& variant, boost::variant<Head2, Tail2...> const& variant2)
{
    return boost::apply_visitor(detail::lambda_wrapped_visitor<Lambda, decltype(std::declval<Lambda>()(std::declval<Head const&>(), std::declval<Head2 const&>()))>{l}, variant, variant2);
}

template<class Lambda, class Head, class... Tail, class Head2, class... Tail2>
inline auto apply_lambda(Lambda const& l, boost::variant<Head, Tail...> const& variant, boost::variant<Head2, Tail2...> &variant2)
{
    return boost::apply_visitor(detail::lambda_wrapped_visitor<Lambda, decltype(std::declval<Lambda>()(std::declval<Head &>(), std::declval<Head2>()))>{l}, variant, variant2);
}

} // namespace variant
} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_VARIANT_HPP_INCLUDED
