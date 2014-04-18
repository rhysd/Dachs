#define BOOST_TEST_MODULE ParserTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/parser.hpp"
#include "dachs/helper/util.hpp"
#include "test_helper.hpp"

#include <string>

#include <boost/test/included/unit_test.hpp>

using namespace dachs::test::parser;

// NOTE: use global variable to avoid executing heavy construction of parser
static dachs::syntax::parser p;

inline void check_parse_no_throw(std::string const& s)
{
    BOOST_CHECK_NO_THROW(p.parse(s));
}

inline void check_parse_throw(std::string const& s)
{
    BOOST_CHECK_THROW(p.parse(s), dachs::syntax::parse_error);
}

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(check_enable_to_parse)
{
    // minimal
    check_parse_no_throw("func main; end");

    // general cases
    check_parse_no_throw(R"(
        func hoge()
        end

        func hoge()
            some_statement
        end

        func hoge()
            ; # empty statement
        end

        func hoge(a)
        end

        func hoge(a, b)
        end

        func hoge(a) : t
        end

        func hoge(a, b) : t
        end

        func hoge(a : int)
        end

        func hoge(a : int, b : int)
        end

        func hoge(a : int) : t
        end

        func hoge(a : int, b : int) : t
        end

        func hoge()
        end

        func hoge(var a)
        end

        func hoge(var a, b)
        end

        func hoge(var a) : t
        end

        func hoge(var a, b) : t
        end

        func hoge(var a : int)
        end

        func hoge(var a : int, b : int)
        end

        func hoge'(a, var b) : t
        end

        func main
        end
        )");

    check_parse_throw(R"(
        func main
        en
        )");

    check_parse_throw(R"(
        func (a, b)
        en
        )");
}

BOOST_AUTO_TEST_CASE(procedure)
{
    // minimal
    check_parse_no_throw("proc p; end");

    check_parse_no_throw(R"(
        proc hoge
        end

        proc hoge()
            some_statement
        end

        proc hoge()
            ; # empty statement
        end

        proc hoge(a)
        end

        proc hoge(a, b)
        end

        proc hoge(a : int)
        end

        proc hoge(a : int, b : int)
        end

        proc hoge()
        end

        proc hoge(var a)
        end

        proc hoge(var a, b)
        end

        proc hoge(var a : int)
        end

        proc hoge(var a : int, b : int)
        end

        proc main
        end
        )");

    check_parse_throw("proc hoge(); en");

    check_parse_throw("proc (a, b); end");

    // procedure shouldn't have return type
    check_parse_throw("proc hoge(a, b) : int; end");
}

BOOST_AUTO_TEST_CASE(comprehensive_cases)
{
    check_no_throw_in_all_cases_in_directory("assets/comprehensive");
}

BOOST_AUTO_TEST_CASE(samples)
{
    check_no_throw_in_all_cases_in_directory("assets/samples");
}

BOOST_AUTO_TEST_SUITE_END()
