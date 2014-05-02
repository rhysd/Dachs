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

namespace dachs {
namespace test {
namespace parser {

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

inline
void check_no_throw_in_all_cases_in_directory(std::string const& dir_name)
{
    dachs::syntax::parser parser;
    check_all_cases_in_directory(dir_name, [&parser](fs::path const& p){
                std::cout << "testing " << p.c_str() << std::endl;
                BOOST_CHECK_NO_THROW(
                    parser.parse(
                        *dachs::helper::read_file<std::string>(p.c_str())
                    )
                );
            });
}

inline
void check_throw_in_all_cases_in_directory(std::string const& dir_name)
{
    dachs::syntax::parser parser;
    check_all_cases_in_directory(dir_name, [&parser](fs::path const& p){
                std::cout << "testing " << p.c_str() << std::endl;
                BOOST_CHECK_THROW(
                    parser.parse(
                        *dachs::helper::read_file<std::string>(p.c_str())
                    )
                    , dachs::parse_error
                );
            });
}

} // namespace parser
} // namespace test
} // namespace dachs

#endif    // DACHS_TEST_TEST_HELPER_HPP_INCLUDED
