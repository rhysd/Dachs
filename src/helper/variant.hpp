#if !defined DACHS_HELPER_VARIANT_HPP_INCLUDED
#define      DACHS_HELPER_VARIANT_HPP_INCLUDED

#include <boost/variant/variant.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>

#include <utility>

namespace dachs {
namespace helper {
namespace variant {

namespace detail {

    template<class T>
    struct static_getter : public boost::static_visitor<boost::optional<T>> {
        typedef boost::optional<T> return_type;

        return_type operator()(T const& val) const
        {
            return val;
        }

        template<class U>
        return_type operator()(U const&) const
        {
            return boost::none;
        }
    };

    template<class T>
    struct type_checker : public boost::static_visitor<bool> {
        bool operator()(T const&) const
        {
            return true;
        }

        template<class U>
        bool operator()(U const&) const
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

} // namespace variant
} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_VARIANT_HPP_INCLUDED
