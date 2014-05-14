#if !defined DACHS_HELPER_VARIANT_HPP_INCLUDED

#define      DACHS_HELPER_VARIANT_HPP_INCLUDED

#include <utility>
#include <type_traits>

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>

namespace dachs {
namespace helper {
namespace variant {

namespace detail {

    template<class T>
    struct static_getter : public boost::static_visitor<boost::optional<T>> {
        typedef boost::optional<T> return_type;

        return_type operator()(T const& val) const noexcept
        {
            return val;
        }

        template<class U>
        return_type operator()(U const&) const noexcept
        {
            return boost::none;
        }
    };

    template<class T>
    struct type_checker : public boost::static_visitor<bool> {
        bool operator()(T const&) const noexcept
        {
            return true;
        }

        template<class U>
        bool operator()(U const&) const noexcept
        {
            return false;
        }
    };

} // namespace detail

template<class T, class... Args>
inline boost::optional<T> get(boost::variant<Args...> const& v)
{
    return boost::apply_visitor(detail::static_getter<T>{}, v);
}

template<class T, class... Args>
inline bool has(boost::variant<Args...> const& v)
{
    return boost::apply_visitor(detail::type_checker<T>{}, v);
}

namespace detail {

template<class F, class Result>
struct lambda_wrapped_visitor : public boost::static_visitor<Result> {

    F const& f;

    explicit lambda_wrapped_visitor(F const& f)
        : f(f)
    {}

    template<class T>
    Result operator()(T const& t) const
    {
        return f(t);
    }
};

template<class F>
struct lambda_wrapped_visitor<F, void> : public boost::static_visitor<void> {
    F const& f;

    explicit lambda_wrapped_visitor(F const& f)
        : f(f)
    {}

    template<class T>
    void operator()(T const& t) const
    {
        f(t);
    }
};

} // namespace detail

template<class Lambda, class Head, class... Tail>
inline auto apply_lambda(Lambda const& v, boost::variant<Head, Tail...> const& variant)
{
    return boost::apply_visitor(detail::lambda_wrapped_visitor<Lambda, typename std::result_of<Lambda(Head)>::type>{v}, variant);
}

} // namespace variant
} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_VARIANT_HPP_INCLUDED
