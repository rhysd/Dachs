#if !defined DACHS_HELPER_UTIL_HPP_INCLUDED
#define      DACHS_HELPER_UTIL_HPP_INCLUDED

#include <iterator>
#include <fstream>
#include <memory>
#include <type_traits>
#include <cstddef>
#include <algorithm>
#include <initializer_list>
#include <boost/optional.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/iterator/zip_iterator.hpp>

namespace dachs {
namespace helper {

template <class String>
inline boost::optional<String> read_file(std::string const& file_name)
{
    typedef typename String::value_type CharT;

    std::basic_ifstream<CharT> input(file_name, std::ios::in);
    if (!input.is_open()) {
        return boost::none;
    }
    return String{std::istreambuf_iterator<CharT>{input},
                  std::istreambuf_iterator<CharT>{}};
}

namespace detail {

template<class T>
struct is_shared_ptr_impl : std::false_type
{};

template<class T>
struct is_shared_ptr_impl<std::shared_ptr<T>> : std::true_type
{};

} // namespace detail

template<class T>
struct is_shared_ptr
    : detail::is_shared_ptr_impl<
          typename std::remove_cv<T>::type
      >
{};

template<bool B, class T = void>
using enable_if = typename std::enable_if<B, T>::type;

template<bool B, class T = void>
using disable_if = typename std::enable_if<!B, T>::type;

// When you don't want to copy result object, use boost::range_reference<T>::type
template<class Range, class Value>
inline boost::optional<typename Range::value_type>
find(Range const& r, Value const& v)
{
    auto const result = boost::find(r, v);
    if (result == boost::end(r)) {
        return boost::none;
    } else {
        return *result;
    }
}

// When you don't want to copy result object, use boost::range_reference<T>::type
template<class Range, class Predicate>
inline boost::optional<typename Range::value_type>
find_if(Range const& r, Predicate const& p)
{
    static_assert(std::is_same<decltype(p(std::declval<typename Range::value_type>())), bool>::value, "predicate doesn't return bool value");
    auto const result = boost::find_if(r, p);
    if (result == boost::end(r)) {
        return boost::none;
    } else {
        return *result;
    }
}

template<class T, class U>
inline bool any_of(std::initializer_list<T> const& list, U const& value)
{
    return std::any_of(std::begin(list), std::end(list), [&value](T const& v){ return v == value; });
}

template<class... Ranges>
inline auto zipped(Ranges &... ranges /*lvalue references*/)
{
    return boost::make_iterator_range(
            boost::make_zip_iterator(boost::make_tuple(boost::begin(ranges)...)),
            boost::make_zip_iterator(boost::make_tuple(boost::end(ranges)...))
        );
}

template<class... Args>
struct are_same;

template<class T>
struct are_same<T> : std::true_type
{};

template<class T, class U>
struct are_same<T, U> : std::is_same<T, U>
{};

template<class T, class U, class... R>
struct are_same<T, U, R...>
    : std::conditional<
        std::is_same<T, U>::value,
        are_same<T, R...>,
        std::false_type
      >::type
{};

} // namespace helper
}  // namespace dachs

#endif    // DACHS_HELPER_UTIL_HPP_INCLUDED
