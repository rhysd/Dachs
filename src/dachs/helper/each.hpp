#if !defined DACHS_HELPER_EACH_HPP_INCLUDED
#define      DACHS_HELPER_EACH_HPP_INCLUDED

#include <utility>
#include <type_traits>
#include <cstddef>

#include <boost/iterator/zip_iterator.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/tuple/tuple.hpp>

namespace dachs {
namespace helper {

enum struct each_result {
    break_,
    continue_,
};

namespace detail {

template<class Predicate, class... Sequences, std::size_t... Indices>
void each_impl(Predicate && p, std::index_sequence<Indices...>, Sequences &... s)
{
    auto begin = boost::make_zip_iterator(boost::make_tuple(boost::begin(s)...));
    auto end = boost::make_zip_iterator(boost::make_tuple(boost::end(s)...));
    for (auto && i : boost::make_iterator_range(begin, end)) {
        if (p(boost::get<Indices>(i)...) == each_result::break_) {
            break;
        }
    }
}

template<class Predicate, class... Sequences, std::size_t... Indices>
void each_impl_void(Predicate && p, std::index_sequence<Indices...>, Sequences &... s)
{
    auto begin = boost::make_zip_iterator(boost::make_tuple(boost::begin(s)...));
    auto end = boost::make_zip_iterator(boost::make_tuple(boost::end(s)...));
    for (auto && i : boost::make_iterator_range(begin, end)) {
        p(boost::get<Indices>(i)...);
    }
}

} // namespace detail

template<class Predicate, class... Sequences>
inline auto each(Predicate && p, Sequences &... s)
    -> typename std::enable_if<
        ! std::is_same<
            decltype(p(*boost::begin(s)...))
            , void
        >::value
    >::type
{
    detail::each_impl(std::forward<Predicate>(p), std::make_index_sequence<sizeof...(Sequences)>{}, s...);
}

template<class Predicate, class... Sequences>
inline auto each(Predicate && p, Sequences &... s)
    -> typename std::enable_if<
        std::is_same<
            decltype(p(*boost::begin(s)...))
            , void
        >::value
    >::type
{
    detail::each_impl_void(std::forward<Predicate>(p), std::make_index_sequence<sizeof...(Sequences)>{}, s...);
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_EACH_HPP_INCLUDED
