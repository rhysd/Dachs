#if !defined DACHS_HELPER_VARIANT_HPP_INCLUDED
#define      DACHS_HELPER_VARIANT_HPP_INCLUDED

#include <utility>
#include <type_traits>

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/get.hpp>
#include <boost/optional.hpp>

namespace dachs {
namespace helper {
namespace variant {

template<class T, class... Args>
inline boost::optional<T> get_as(boost::variant<Args...> const& v)
{
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

} // namespace variant
} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_VARIANT_HPP_INCLUDED
