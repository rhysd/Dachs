#if !defined DACHS_HELPER_UTIL_HPP_INCLUDED
#define      DACHS_HELPER_UTIL_HPP_INCLUDED

#include <iterator>
#include <fstream>
#include <memory>
#include <type_traits>
#include <boost/optional.hpp>
#include <boost/range/algorithm/find.hpp>
#include <boost/range/algorithm/find_if.hpp>

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

} // namespace helper
}  // namespace dachs

#endif    // DACHS_HELPER_UTIL_HPP_INCLUDED
