#define BOOST_TEST_MODULE ParserTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/parser.hpp"
#include "dachs/helper/util.hpp"
#include "test_helper.hpp"

#include <boost/test/included/unit_test.hpp>

using namespace dachs::test::parser;

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(check_enable_to_parse)
{
    check_no_throw_in_all_cases_in_directory("assets/function/no_throw");
    check_throw_in_all_cases_in_directory("assets/function/throw");
}

BOOST_AUTO_TEST_CASE(procedure)
{
    check_no_throw_in_all_cases_in_directory("assets/procedure/no_throw");
    check_throw_in_all_cases_in_directory("assets/procedure/throw");
}

BOOST_AUTO_TEST_SUITE_END()
