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

// Fallback for libstdc++ 4.8
#if (defined(__GLIBCPP__) || defined(__GLIBCXX__)) && (__GNUC__ == 4 && __GNUC_MINOR__ <= 8)

template <std::size_t... Indices>
struct index_sequence {
};

template <std::size_t Start, std::size_t Last, std::size_t Step = 1,
          class Acc = index_sequence<>, bool Finish = (Start >= Last)>
struct index_range {
    typedef Acc type;
};

template <std::size_t Start, std::size_t Last, std::size_t Step,
          std::size_t... Indices>
struct index_range<Start, Last, Step, index_sequence<Indices...>, false>
    : index_range<Start + Step, Last, Step, index_sequence<Indices..., Start>> {
};

template <std::size_t Start, std::size_t Last, std::size_t Step = 1>
using idx_range = typename index_range<Start, Last, Step>::type;

template <std::size_t Size>
using make_index_sequence = typename index_range<0, Size, 1>::type;

#else

using std::index_sequence;
using std::make_index_sequence;

#endif

template<class Predicate, class... Sequences, std::size_t... Indices>
void each_impl(Predicate && p, index_sequence<Indices...>, Sequences &... s)
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
void each_impl_void(Predicate && p, index_sequence<Indices...>, Sequences &... s)
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
    detail::each_impl(std::forward<Predicate>(p), detail::make_index_sequence<sizeof...(Sequences)>{}, s...);
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
    detail::each_impl_void(std::forward<Predicate>(p), detail::make_index_sequence<sizeof...(Sequences)>{}, s...);
}

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_EACH_HPP_INCLUDED
