#define BOOST_TEST_MODULE ParserTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/parser.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/ast_walker.hpp"
#include "test_helper.hpp"

#include <string>

#include <boost/test/included/unit_test.hpp>

using namespace dachs::test::parser;

// NOTE: use global variable to avoid executing heavy construction of parser
static dachs::syntax::parser p;

// TODO: More checks
struct test_visitor {
    template<class T, class W>
    void visit(T &, W const& w)
    {
        w();
    }
};

inline void validate(dachs::ast::ast const& a)
{
    auto root = a.root;
    dachs::ast::walk_topdown(root, test_visitor{});
}

#define CHECK_PARSE_THROW(...) BOOST_CHECK_THROW(p.parse((__VA_ARGS__)), dachs::parse_error)

BOOST_AUTO_TEST_SUITE(parser)

BOOST_AUTO_TEST_CASE(comment)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
            # line comment
            # block comment #
            # escapable \# hoge huga

            #
            # main function
            #
            func main(#tsura#poyo)
                expr # poyo
                #hoge# this_is_expr
            end
        )")));

    CHECK_PARSE_THROW(R"(
            # Line comment is not continued
            to next line
            func main
            end
        )");
}

BOOST_AUTO_TEST_CASE(function)
{
    // minimal
    BOOST_CHECK_NO_THROW(validate(p.parse("func main; end")));

    // general cases
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
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

        func hoge(a,
                  b)
        end

        func hoge(a) : t
        end

        func hoge(a) :
                very_very_long_type_name
        end

        func hoge(a)
                : very_very_long_type_name
        end

        func hoge(a, b) : t
        end

        func hoge(
                    a,
                    b
                ) : t
        end

        func hoge(
                    a,
                    b,
                ) : t
        end

        func hoge(
                    a
                  , b
                  , c
                ) : t
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

        # Operators

        func +(v)
        end

        func +(l, r)
        end

        func *(l, r)
        end

        func /(l, r)
        end

        func %(l, r)
        end

        func <(l, r)
        end

        func >(l, r)
        end

        func &(l, r)
        end

        func ^(l, r)
        end

        func |(l, r)
        end

        func <=(l, r)
        end

        func >=(l, r)
        end

        func ==(l, r)
        end

        func !=(l, r)
        end

        func >>(l, r)
        end

        func <<(l, r)
        end

        func &&(l, r)
        end

        func ||(l, r)
        end

        func ..(l, r)
        end

        func ...(l, r)
        end

        func main
        end
        )")));

    CHECK_PARSE_THROW(R"(
        func main
        en
        )");

    CHECK_PARSE_THROW(R"(
        func (a, b)
        en
        )");
}

BOOST_AUTO_TEST_CASE(procedure)
{
    // minimal
    BOOST_CHECK_NO_THROW(validate(p.parse("proc p; end")));

    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
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

        proc hoge(a
                , b)
        end

        proc hoge(a,
                  b)
        end

        proc hoge(a : int)
        end

        proc hoge(a : int, b : int)
        end

        proc hoge(a :
                    int
                , b :
                    int)
        end

        proc hoge(a
                    : int
                , b
                    : int)
        end

        proc hoge(a
                    : int,
                  b
                    : int)
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
        )")));

    CHECK_PARSE_THROW("proc hoge(); en");

    CHECK_PARSE_THROW("proc (a, b); end");
}

BOOST_AUTO_TEST_CASE(literals)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
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
            '\t'
            '\\'
            ' '

            # string
            "aaaaa"
            "bb1239aa12343#$#!!"
            "\"aaa\""
            "\nhoge\nbbb\n"
            "\\aaa\\"
            ""
            "include white spaces"
            "\n\b\f\t\\"

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
            20194890
            1u #unsigned
            10u

            # array
            [1, 10, 100, 1000, 10000]
            [
                1,
                10,
                100,
                1000,
                10000
            ]
            [
                1,
                10,
                100,
                1000,
                10000,
            ]
            [
                  1
                , 10
                , 100
                , 1000
                , 10000
            ]
            [1,
             10,
             100,
             1000,
             10000]
            [1,
             10,
             100,
             1000,
             10000,]
            [1]
            [2.14, 5.15]
            []

            # tuple
            (1, 'a', "aaaa")
            (1,
             'a',
             "aaaa")
            (
                1,
                'a',
                "aaaa"
            )
            (
                1
                , 'a'
                , "aaaa"
            )
            (1, 10)
            ()

            # symbol
            :hogehoge
            :aaa
            :to_s
            :inu
            :answer_is_42

            # dict
            {10 => 'a', 100 => 'b'}
            {
                10 => 'a',
                100 => 'b'
            }
            {10 => 'a',
             100 => 'b'}
            {"aaaa" => :aaa, "bbb" => :bbb}
            {10 => 'a', 100 => 'b',}
            {"aaaa" => :aaa, "bbb" => :bbb,}
            {}
            {3.14 => :pi}
        end
        )")));

    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
            func main
                [(42, 'a'), (53, 'd')]
                ([42, 13, 22], {:aaa => :BBB}, (42, [42, 42], 42), "aaaa", ["aaa", "bbb", "ccc"])
                ([42,
                  13,
                  22],
                 {:aaa => :BBB},
                 (42,
                  [42,
                  42],
                  42),
                 "aaaa",
                 ["aaa",
                  "bbb",
                  "ccc"])
            end
        )")));

    CHECK_PARSE_THROW("func main; 'aaaa' end");
    CHECK_PARSE_THROW("func main; '' end");
    CHECK_PARSE_THROW("func main; ''' end");
    CHECK_PARSE_THROW("func main; 43. end");
}

BOOST_AUTO_TEST_CASE(postfix_expr)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            foo.awesome_member_func
            foo.
                awesome_member_func
            foo
                .awesome_member_func
            foo[index]
            foo[
                    23 * 4 >> 5
               ]
            foo(
                    function,
                    call
                )
            foo(
                    function,
                    call,
                )
            foo(function,
                call,
                newline)
            foo()
            foo(a)

            foo.bar(args)[3]
            foo[3].bar.baz(args)
            foo(hoge).bar[42]
        end
        )")));

    CHECK_PARSE_THROW("func main; foo[42 end");
    CHECK_PARSE_THROW("func main; foo(42 end");
    CHECK_PARSE_THROW("func main; foo(42,a end");
    CHECK_PARSE_THROW("func main; foo(hoge.hu end");
}

BOOST_AUTO_TEST_CASE(type)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            expr : int
            expr : string
            expr : float
            expr : (float)
            expr : (
                    float
                   )
            expr : [int]
            expr : [
                       int
                   ]
            expr : {int => string}
            expr
                : {int => string}
            expr :
                {int => string}
            expr : {
                       int => string
                   }
            expr : {
                       int
                           =>
                       string
                   }
            expr : (int, char)
            expr : (int,
                    char)
            expr : (
                       int,
                       char
                   )

            expr : (
                       int
                     , char
                   )
            expr : (
                       int,
                       char,
                   )
            expr : ()
            expr : [(int)] # it means [int]
            expr : (int, [string], {() => [int]}, (float, [int]))
            expr : [{([(int, string)]) => string}]

            expr : func() : int
            expr : proc()
            expr : func(int, aaa) : int
            expr : func(
                    int,
                    aaa
                    )
                     :
                       int
            expr : func(
                      int
                    , aaa
                    )
                     :
                       int
            expr : func(
                    int,
                    aaa,
                    )
                     :
                       int
            expr : proc(int, aaa)
            expr : proc(
                           int,
                           aaa
                       )
            expr : proc(
                           int
                         , aaa
                       )
            expr : proc(
                           int,
                           aaa,
                       )
            expr : [func() : int]
            expr : (func(int) : string, proc(int), [func() : int])
            expr : {func(char) : int => proc(string)}

            expr : int?
            expr : string?
            expr : float?
            expr : [int]?
            expr : [int?]?
            expr : {int => string}?
            expr : {int => string?}?
            expr : (int?, char)?
            expr : ()?
            expr : [(int)?] # it means [int]
            expr : (int?, [string?], {()? => [int?]?}?, (float, [int]?)?)?
            expr : [{([(int, string?)?]?)? => string}?]?

            expr : (func() : int)?
            expr : func() : int? # it returns maybe int
            expr : (proc())?
            expr : (func(int, aaa) : int)?
            expr : (proc(int, aaa))?
            expr : [(func() : int)?]
            expr : ((func(int) : string)?, (proc(int))?, [func() : int]?)
            expr : {(func(char) : int)? => (proc(string))?}?

            # template types
            expr : T(int)
            expr : T(
                        int
                    )
            expr : T(int, string)
            expr : [T(int)]
            expr : (T(int), U(int))
            expr : {T(int) => U(int)}
            expr : T(int)?
            expr : T(int?, string?)
            expr : [T(int)?]
            expr : (T(int)?, U(int)?)
            expr : {T(int)? => U(int)?}?
        end
        )")));

    CHECK_PARSE_THROW("func main; expr : proc() : int end # one element tuple is not allowed");
    CHECK_PARSE_THROW("func main; expr : proc() : int end # must not have return type");
    CHECK_PARSE_THROW("func main; expr : func() end # must have return type");
    CHECK_PARSE_THROW("func main; expr : T() end # must have return type");
    CHECK_PARSE_THROW("func main; expr : [T](int) end # must have return type");
    CHECK_PARSE_THROW("func main; expr : (T)(int) end # must have return type");
}

BOOST_AUTO_TEST_CASE(primary_expr)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            (1 + 2 * 3)
            (
                1 + 2 * 3
            )
            hogehoge # variable reference
            int{42}
            (int, int){42, 42}
            (int,
             int){42,
                  42}
            {int => string}{{1 => "aaa", 2 => "bbb"}}
        end
        )")));

    CHECK_PARSE_THROW("func main; (1 + 2; end");
    CHECK_PARSE_THROW("func main; int{42; end");
}

BOOST_AUTO_TEST_CASE(unary_expr)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            -42
            +42
            ~42
            !true
            -+~42
            !!true
        end
        )")));
}

BOOST_AUTO_TEST_CASE(cast_expression)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            expr as int
            expr as int
            expr as int?
            expr as [int]
            expr as (int, int)?
            expr as T((int, int)?)
            expr
                as T((int, int)?)
            expr as
                T((int, int)?)
        end
        )")));
}

BOOST_AUTO_TEST_CASE(binary_expression)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            1 + 1
            1 - 1

            1
            +
            1

            1
            -
            1

            1 * 1
            1 / 1
            1 % 1

            1
            *
            1

            1
            /
            1

            1
            %
            1

            1 < 1
            1 > 1

            1
            <
            1

            1
            >
            1

            1 & 1
            1 ^ 1
            1 | 1

            1
            &
            1

            1
            ^
            1

            1
            |
            1

            1 <= 1
            1 >= 1

            1
            <=
            1

            1
            >=
            1

            1 == 1
            1 != 1

            1
            ==
            1

            1
            !=
            1

            1 >> 1
            1 << 1

            1
            >>
            1

            1
            <<
            1

            true && true
            true || true

            true
            &&
            true

            true
            ||
            true

            1..2
            1...3

            1
            ..
            2

            1
            ...
            3

            1 = 1
            1 += 1
            1 -= 1
            1 *= 1
            1 /= 1
            1 %= 1
            1 |= 1
            1 &= 1
            1 ^= 1
            1 <= 1
            1 >= 1
            1 >>= 1
            1 <<= 1

            1 + 2 * 3 - 4 / 5 % 6 & 7 ^ 9 | 10 >> 11 << 12
            1 + (2 * (3 - 4) / 5) % 6 & 7 ^ 9 | (10 >> 11) << 12

            1 < 3 || 4 > 5 && 6 == 7 || 8 != 9
            1 < 3 || (4 > 5) && (6 == 7) || 8 != 9
        end
        )")));

    CHECK_PARSE_THROW("func main 1 == end");
    CHECK_PARSE_THROW("func main 1 + end");
    CHECK_PARSE_THROW("func main true && end");
}

BOOST_AUTO_TEST_CASE(assignment_expr)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            aaa = 42
            aaa, bbb = 42, 31
            aaa,
            bbb = 42,
                  31
            aaa, bbb = do_something()
        end
        )")));
}

BOOST_AUTO_TEST_CASE(if_expr)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            (if true then 42 else 24)
            hoge(if true then 3.14 else 4.12)
            (if true then
                42
            else
                24)
            (if true then 42
             else 24)
            (if if then if else if) # 'if' is a contextual keyword
        end
        )")));

    // it is parsed as if statement and it will fail
    CHECK_PARSE_THROW("func main if true then 42 else 24 end");
}

BOOST_AUTO_TEST_CASE(object_construct)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            int{42}
            int{
                42
               }
            [int]{
                    [
                        1,
                        2,
                        3,
                    ]
                 }
            {int => string}{{42 => "answer"}}
        end
        )")));
}

BOOST_AUTO_TEST_CASE(variable_decl)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            a := 42
            var a := 42
            a := int{42}
            a, b := 42, 24
            a,
            b := 42,
                 24
            var a, b := 'a', 'b'
            var a, var b := 'a', 'b'
            var a,
                b := 'a',
                     'b'
            var a,
                b,
                   :=
                       'a',
                       'b'
            var a
              , b := 'a'
                    ,'b'
            a, b := foo()
            var a, b := bar()
            var a, b := int{32}, char{'b'}
            var a, b := [] : [int], {} : {int => string}
            var a,
                b := [] : [int],
                     {} : {int => string}

            var a : int := 42
            var a :
                int := 42
        end
        )")));

    CHECK_PARSE_THROW("func main var a := b, end");
}

BOOST_AUTO_TEST_CASE(return_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            return
            return 42
            return 42, 'a', "bbb"
            return 42,
                   'a',
                   "bbb"
            return 42
                 , 'a'
                 , "bbb"
        end
        )")));
}

BOOST_AUTO_TEST_CASE(constant_decl)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        a := 42
        a := int{42}
        a, b := 42, 24
        a,
        b := 42,
                24
        a,
        b := 'a',
             'b'
        a,
        b,
            :=
                'a',
                'b'
        a
        , b := 'a'
              ,'b'
        a, b := foo()
        a, b := bar()
        a, b := int{32}, char{'b'}
        a, b := [] : [int], {} : {int => string}
        a,
        b := [] : [int],
                {} : {int => string}

        a : int := 42
        a :
        int := 42
        )")));

    CHECK_PARSE_THROW("a := b,");
}

BOOST_AUTO_TEST_CASE(if_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            if aaaa
                expr
            end

            if aaaa then
                expr
            end

            if aaaa
                expr1
            else
                expr2
            end

            if aaaa then
                expr1
            else
                expr2
            end

            if aaaa then 42 else 52 end

            if aaa
                expr
            elseif bbb
                expr
            elseif ccc
                expr
            end

            if aaa
                expr
            elseif bbb
                expr
            elseif ccc
                expr
            else
                expr
            end

            if aaa then
                expr
            elseif bbb then
                expr
            elseif ccc then
                expr
            else
                expr
            end

            if aaa then bbb elseif bbb then expr elseif ccc then expr else ddd end

            if aaaa then bbb end

            if aaaa then bbb else ddd end
        end
        )")));

    CHECK_PARSE_THROW("func main if aaa then bbb else ccc end");
}


BOOST_AUTO_TEST_CASE(switch_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            case aaa
            when true
                hoge
            end

            case aaa
            when true then  poyo
            when false then hoge
            else            huga
            end

            case shinchoku
            when arimasu
                doudesuka
            else
                jigokukakokoha
            end

            case aaa
            when true, false
                hoge
            end
        end
        )")));

    CHECK_PARSE_THROW(R"(
        func main
            case aaa
            else
                bbb
            end
        end
        )");

    CHECK_PARSE_THROW(R"(
        func main
            case aaa
            end
        end
        )");
}

BOOST_AUTO_TEST_CASE(case_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            case
            when true
                hoge
            end

            case
            when a == 1
                expr
            else
                expr
            end

            case
            when true then
                hoge
            end

            case
            when a == 1 then
                expr
            else
                expr
            end

            case
            when a == 1 then expr
            else             expr
            end
        end
        )")));

    CHECK_PARSE_THROW(R"(
        func main
            case
            end
        end
        )");

    CHECK_PARSE_THROW(R"(
        func main
            case
            else
                bbb
            end
        end
        )");

    CHECK_PARSE_THROW(R"(
        func main
            case
            when aaa == 1, b == 2
            end
        end
        )");
}

BOOST_AUTO_TEST_CASE(for_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            for a in arr
                moudameda
            end

            for a in arr do
                moudameda
            end

            for a, b in arr
                madaikeru
            end

            for var a, var b in arr
                madaikeru
            end

            for var a : int, var b : char in arr
                madadameda
            end
        end
        )")));
}

BOOST_AUTO_TEST_CASE(while_statement)
{
    BOOST_CHECK_NO_THROW(validate(p.parse(R"(
        func main
            for true
                moudameda
            end

            for true do
                moudameda
            end

            for true : bool
                madadameda
            end
        end
        )")));
}

BOOST_AUTO_TEST_CASE(ast_nodes_node_illegality)
{
    dachs::syntax::parser p;
    for (auto const& d : {"assets/comprehensive", "assets/samples"}) {
        check_all_cases_in_directory(d, [&p](fs::path const& path){
                    std::cout << "testing " << path.c_str() << std::endl;
                    auto ast = p.parse(
                                *dachs::helper::read_file<std::string>(path.c_str())
                           );
                    validate(ast);
                });
    }
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
