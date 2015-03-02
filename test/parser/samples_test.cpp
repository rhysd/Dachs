#define BOOST_TEST_MODULE ParserSamplesTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"

#include "dachs/parser/parser.hpp"
#include "dachs/exception.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/helper/util.hpp"

#include <string>

#include <boost/test/included/unit_test.hpp>

using namespace dachs::test;

// NOTE: use global variable to avoid executing heavy construction of parser
struct test_visitor {
    template<class T, class W>
    void visit(T &, W const& w)
    {
        w();
    }
};

inline
void check_no_throw_in_all_cases_in_directory(std::string const& dir_name)
{
    dachs::syntax::parser parser;
    check_all_cases_in_directory(dir_name, [&parser](fs::path const& p){
                std::cout << "testing " << p.c_str() << std::endl;
                BOOST_CHECK_NO_THROW(
                    parser.check_syntax(
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
                    parser.check_syntax(
                        *dachs::helper::read_file<std::string>(p.c_str())
                    )
                    , dachs::parse_error
                );
            });
}


BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_SUITE(samples)

BOOST_AUTO_TEST_CASE(comprehensive_cases)
{
    check_no_throw_in_all_cases_in_directory(DACHS_ROOT_DIR "/test/assets/comprehensive");
}

BOOST_AUTO_TEST_CASE(samples)
{
    check_no_throw_in_all_cases_in_directory(DACHS_ROOT_DIR "/test/assets/samples");
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
