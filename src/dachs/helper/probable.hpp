#if !defined DACHS_HELPER_PROBABLE_HPP_INCLUDED
#define      DACHS_HELPER_PROBABLE_HPP_INCLUDED

#include <string>
#include <type_traits>
#include <boost/variant/variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/optional.hpp>
#include <boost/format.hpp>

// Note:
// dachs::helper::probable represents either a specified type or an error(usually std::string).
// This class is used for a returned value which is the result of maybe-failed computation.
// This is similar to Boost.Expected and should be replaced with it when it will be released.

namespace dachs {
namespace helper {

namespace detail {
    // Note:
    // This class can't be in probable<T, E> because T must be specified
    // in oops() free function.
    template<class Err>
    struct Error {
        Err value;

        bool operator==(Error const& rhs) const
        {
            return value == rhs.value;
        }

        bool operator!=(Error const& rhs) const
        {
            return value != rhs.value;
        }

        bool operator<(Error const& rhs) const
        {
            return value < rhs.value;
        }
    };
} // namespace detail

template<class T, class E = std::string>
class probable {
    static_assert(std::is_default_constructible<E>::value, "E must be default constructible");
    using error_type = detail::Error<E>;
    using element_type = T;
    using value_type = boost::variant<element_type, error_type>;
    value_type value;

public:

    using success_type = T;
    using failure_type = E;

    probable()
        : value(error_type{})
    {}

    probable(probable && p)
        : value(std::move(p).value)
    {}

    probable(probable const& p)
        : value(p.value)
    {}

    template<class U>
    probable(probable<U, E> & p)
        : value(p.raw_value())
    {}

    template<class U>
    probable(probable<U, E> && p)
        : value(std::move(p.raw_value()))
    {}

    template<class U>
    probable(probable<U, E> const& p)
        : value(p.raw_value())
    {}

    template<class U>
    probable(U && v)
        : value(std::forward<U>(v))
    {}

    probable &operator=(probable && rhs)
    {
        value = std::move(rhs).value;
        return *this;
    }

    probable &operator=(probable const& rhs)
    {
        value = rhs.value;
        return *this;
    }

    template<class U>
    probable &operator=(U && rhs)
    {
        value = std::forward<U>(rhs);
        return *this;
    }

    bool operator==(probable const& rhs) const
    {
        return value == rhs.value;
    }

    template<class U>
    bool operator==(U const& rhs) const
    {
        return value == rhs;
    }

    template<class U>
    bool operator!=(U const& rhs) const
    {
        return !(*this == rhs);
    }

    bool operator<(probable const& rhs) const
    {
        return value < rhs.value;
    }

    template<class U>
    bool operator<(U const& rhs) const
    {
        return value < rhs;
    }

    bool failure() const
    {
        return value.which() == 1;
    }

    bool success() const
    {
        return value.which() == 0;
    }

    boost::optional<element_type> get() const
    {
        element_type const* const ptr = boost::get<element_type>(&value);
        if (ptr) {
            return {*ptr};
        } else {
            return boost::none;
        }
    }

    element_type &get_unsafe()
    {
        auto *const ptr = boost::get<element_type>(&value);
        assert(ptr);
        return *ptr;
    }

    element_type const& get_unsafe() const
    {
        auto const* const ptr = boost::get<element_type>(&value);
        assert(ptr);
        return *ptr;
    }

    boost::optional<E> get_error() const
    {
        error_type const* const ptr = boost::get<error_type>(&value);
        if (ptr) {
            return {ptr->value};
        } else {
            return boost::none;
        }
    }

    E &get_error_unsafe()
    {
        auto *const ptr = boost::get<error_type>(&value);
        assert(ptr);
        return ptr->value;
    }

    E const& get_error_unsafe() const
    {
        auto const* const ptr = boost::get<error_type>(&value);
        assert(ptr);
        return ptr->value;
    }

    value_type &raw_value()
    {
        return value;
    }

    value_type const& raw_value() const
    {
        return value;
    }

    boost::variant<element_type, E> get_value_or_error() const
    {
        if (auto const* v = boost::get<element_type>(&value)) {
            return *v;
        } else if (auto const* v = boost::get<E>(&value)) {
            return *v;
        } else {
            return {};
        }
    }
};

template<class T, class E = std::string>
inline probable<T, E> probably(T && value)
{
    return probable<T, E>(std::forward<T>(value));
}

template<class T, class... Args>
inline probable<T, std::string> make_probable(Args &&... args)
{
    return probable<T, std::string>(T(std::forward<Args>(args)...));
}

template<class S>
inline typename std::enable_if<
        std::is_constructible<std::string, decltype(std::forward<S>(std::declval<S>()))>::value,
        detail::Error<std::string>
    >::type
oops(S && err)
{
    return detail::Error<std::string>{std::forward<S>(err)};
}

template<class E>
inline typename std::enable_if<
        !std::is_constructible<std::string, decltype(std::forward<E>(std::declval<E>()))>::value,
        detail::Error<E>
    >::type
oops(E && e)
{
    return detail::Error<E>{std::forward<E>(e)};
}

template<class E = std::string, class E1, class E2, class... EN>
inline detail::Error<E> oops(E1 && e1, E2 && e2, EN &&... en)
{
    return detail::Error<E>{std::forward<E1>(e1), std::forward<E2>(e2), std::forward<EN>(en)...};
}

namespace detail {

// Note:
// Fold expression is available if C++17 is enabled.
//
//      boost::format(fmt) % ... % args

template<class Format>
inline auto oops_fmt_impl(Format && fmt)
{
    return std::forward<Format>(fmt);
}

template<class Format, class Head, class... Tail>
inline auto oops_fmt_impl(Format && fmt, Head && h, Tail &&... t)
{
    return oops_fmt_impl(std::forward<Format>(fmt) % std::forward<Head>(h), std::forward<Tail>(t)...);
}
} // namespace detail

template<class Format, class... Args>
inline detail::Error<std::string> oops_fmt(Format && fmt, Args &&... args)
{
    return detail::Error<std::string>{
        detail::oops_fmt_impl(
            boost::format(std::forward<Format>(fmt)),
            std::forward<Args>(args)...
        ).str()
    };
}

template<class T>
auto make_probable_generator()
{
    return [](auto &&... args){
        return make_probable<T>(std::forward<decltype(args)>(args)...);
    };
}

// Note:
// Other utilities are considered but not implemented yet because they are not necessary at least now.
// e.g.
//      flat_map(probable, predicate)
//      operator>= as monadic binding

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_PROBABLE_HPP_INCLUDED
