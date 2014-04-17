#define BOOST_TEST_MODULE ParserTest

#include <string>

#include "dachs/parser.hpp"

#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>

/*
 * TODO:
 * should describe test code of Dachs in another file.
 * otherwise, this test code must be compiled when the test code is modified.
 */

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(function)
{
    std::string s;
    dachs::syntax::parser p;

    s = R"(func main; end)";
    BOOST_CHECK_NO_THROW(p.parse(s));

    s = R"(
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

        func main
        end
        )";
    BOOST_CHECK_NO_THROW(p.parse(s));
}

BOOST_AUTO_TEST_CASE(procedure)
{
    std::string s;
    dachs::syntax::parser p;

    s = R"(proc p; end)";
    BOOST_CHECK_NO_THROW(p.parse(s));

    s = R"(
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
        )";
    BOOST_CHECK_NO_THROW(p.parse(s));

    // procedure doesn't have return type
    s = R"(proc hoge : int; end)";
    BOOST_CHECK_THROW(p.parse(s), dachs::syntax::parse_error);
}

BOOST_AUTO_TEST_SUITE_END()
