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

template<class Predicate>
inline
void check_all_cases_in_directory(std::string const& dir_name, Predicate const& predicate)
{
    // Note:
    // I can't use Boost.Range adaptors because directory_iterator doesn't meet forward traversal.
    for (auto const& entry
            : boost::make_iterator_range(
                fs::directory_iterator{fs::path{dir_name}},
                fs::directory_iterator{}
            )
    ) {
        if (!fs::is_directory(entry)) {
            predicate(entry);
        }
    }
}

} // namespace test
} // namespace dachs

#endif    // DACHS_TEST_TEST_HELPER_HPP_INCLUDED
