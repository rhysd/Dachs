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

BOOST_AUTO_TEST_CASE(comment)
{
    check_parse_no_throw(R"(
            # line comment
            # block comment #

            #
            # main function
            #
            func main(#tsura#poyo)
                expr # poyo
                #hoge# this_is_expr
            end
        )");

    check_parse_throw(R"(
            # Line comment is not continued
            to next line
            func main
            end
        )");
}

BOOST_AUTO_TEST_CASE(function)
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

        func is_true?(b)
            return b
        end

        func shinchoku_arimasu?(b)
            return false
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

BOOST_AUTO_TEST_CASE(literals)
{
    check_parse_no_throw(R"(
        func main
            # character
            'a'
            'b'
            'Z'
            '9'
            '\n'
            '\''
            '\b'
            '\f'
            '\\'

            # string
            "aaaaa"
            "bb1239aa12343#$#!!"
            "\"aaa\""
            "\nhoge\nbbb\n"
            "\\aaa\\"
            ""

            # boolean
            true
            false

            # float
            3.14
            0.5
            10.0
            1.0e10
            -1.0e10
            -3.14
            -0.5
            -5.0

            # integer
            1
            42
            -1
            -42
            201948903843
            1u #unsigned
            10u

            # array
            [1, 10, 100, 1000, 10000]
            [1]
            [2.14, 5.15]
            []

            # tuple
            (1, 'a', "aaaa")
            (1, 10)
            ()

            # symbol
            :hogehoge
            :aaa
            :to_s
            :inu
            :answer_is_42

            # map
            {10 => 'a', 100 => 'b'}
            {"aaaa" => :aaa, "bbb" => :bbb}
            {10 => 'a', 100 => 'b',}
            {"aaaa" => :aaa, "bbb" => :bbb,}
            {}
            {3.14 => :pi}
        end
        )");

    check_parse_no_throw(R"(
            func main
                [(42, 'a'), (53, 'd')]
                ([42, 13, 22], {:aaa => :BBB}, (42, [42, 42], 42), "aaaa", ["aaa", "bbb", "ccc"])
            end
        )");

    check_parse_throw("func main; 'aaaa' end");
    check_parse_throw("func main; '' end");
    check_parse_throw("func main; ''' end");
    check_parse_throw("func main; 43. end");
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
