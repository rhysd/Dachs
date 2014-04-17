#if !defined DACHS_HELPER_UTIL_HPP_INCLUDED
#define      DACHS_HELPER_UTIL_HPP_INCLUDED

#include <iterator>
#include <fstream>
#include <boost/optional.hpp>

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

} // namespace helper
}  // namespace dachs

#endif    // DACHS_HELPER_UTIL_HPP_INCLUDED
