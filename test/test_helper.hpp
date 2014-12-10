#if !defined DACHS_TEST_TEST_HELPER_HPP_INCLUDED
#define      DACHS_TEST_TEST_HELPER_HPP_INCLUDED

#include <string>
#include <iostream>

#include <boost/test/included/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include <boost/range/adaptors.hpp>

#include "dachs/helper/colorizer.hpp"

struct initializer_before_all_tests {
    initializer_before_all_tests()
    {
        dachs::helper::colorizer::enabled = false;
    }
};

BOOST_GLOBAL_FIXTURE(initializer_before_all_tests);

namespace dachs {
namespace test {

namespace fs = boost::filesystem;
using boost::adaptors::filtered;

inline
auto traverse_directory_range(std::string const& dir_name)
{
    fs::path const p = dir_name;
    return boost::make_iterator_range(fs::directory_iterator{p}, fs::directory_iterator{});
}

template<class Predicate>
inline
void check_all_cases_in_directory(std::string const& dir_name, Predicate const& predicate)
{
    boost::for_each(
        traverse_directory_range(dir_name)
            | filtered([](auto const& d){
                    return !fs::is_directory(d);
                })
        , predicate
    );
}

} // namespace test
} // namespace dachs

#endif    // DACHS_TEST_TEST_HELPER_HPP_INCLUDED
