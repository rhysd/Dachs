#define BOOST_TEST_MODULE ParserTest
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
static dachs::syntax::parser p;

// TODO: More checks
struct test_var_searcher {
    bool found = false;

    template<class W>
    void visit(dachs::ast::node::parameter const& p, W const&)
    {
        if (p->is_var) {
            found = true;
        }
    }

    template<class W>
    void visit(dachs::ast::node::variable_decl const& v, W const&)
    {
        if (v->is_var) {
            found = true;
        }
    }

    template<class T, class W>
    void visit(T const&, W const& w)
    {
        w();
    }
};

struct test_as_searcher {
    bool found = false;

    template<class W>
    void visit(dachs::ast::node::cast_expr const&, W const&)
    {
        found = true;
    }

    template<class T, class W>
    void visit(T const&, W const& w)
    {
        w();
    }
};

#define CHECK_PARSE_THROW(...) BOOST_CHECK_THROW(p.parse((__VA_ARGS__), "test_file"), dachs::parse_error)



BOOST_AUTO_TEST_SUITE(parser)

BOOST_AUTO_TEST_CASE(comment)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
            # line comment
            # escapable \# hoge huga

            #{
                This is a block comment
            }#

            #{
                Escaped \}# is skipped
            }#

            #{
                } corner case 1
            }#

            #{
                } # corner case 2
            }#

            #
            # main function
            #
            func main(#{tsura}# poyo)
                expr # poyo
                foo := #{hoge}# this_is_expr
            end
        )"));

    CHECK_PARSE_THROW(R"(
            # Line comment is not continued
            to next line
            func main
            end
        )");

    CHECK_PARSE_THROW(R"(
            #{
                Non-closed block comment
                occurs parse error.
        )");

}

BOOST_AUTO_TEST_CASE(function)
{
    // minimal
    BOOST_CHECK_NO_THROW(p.check_syntax("func main; end"));

    // general cases
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func hoge()
        end

        func hoge()
            some_statement
        end

        func hoge()
            ; # empty statement
        end

        func hoge'()
        end

        func hoge?()
        end

        func hoge?'()
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
            ret b
        end

        func shinchoku_arimasu?(b)
            ret false
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

        func [](l, r)
        end

        # Keyword corner cases
        funchoge := 42

        func endhoge
            endhuga
        end

        func main
        end
        )"));

    {
        auto a = p.parse(R"(
                func main(varhoge)
                end
            )", "test_file");
        test_var_searcher s;
        dachs::ast::walk_topdown(a.root, s);
        BOOST_CHECK(!s.found);
    }

    CHECK_PARSE_THROW(R"(
        func main
        en
        )");

    CHECK_PARSE_THROW(R"(
        func (a, b)
        en
        )");

    CHECK_PARSE_THROW(R"(
        funcmain
        end
        )");

    CHECK_PARSE_THROW("func hoge'?; end");
    CHECK_PARSE_THROW("func hoge!; end");
}

BOOST_AUTO_TEST_CASE(procedure)
{
    // minimal
    BOOST_CHECK_NO_THROW(p.check_syntax("proc p; end"));

    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

        # Keyword corner cases
        prochoge := 42

        proc endhoge
            endhuga
        end

        proc main
        end
        )"));

    CHECK_PARSE_THROW("proc hoge(); en");

    CHECK_PARSE_THROW("proc (a, b); end");

    CHECK_PARSE_THROW("procmain; end");
}

BOOST_AUTO_TEST_CASE(variable_name)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            aaa
            aaa_bbb
            aaa123
            _aaa
            aaa'
            aaa_'
        end
        )"));
}

BOOST_AUTO_TEST_CASE(literals)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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
            '\v'
            '\e'
            '\\'
            '\0'
            ' '

            # string
            "aaaaa"
            "bb1239aa12343#$#!!"
            "\"aaa\""
            "\nhoge\nbbb\n"
            "\\aaa\\"
            ""
            "include white spaces"
            "\n\b\f\t\v\e\\"

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

            0b010101
            0b010101u
            0x123abc
            0x123abcu
            0o01234567
            0o01234567u

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
        )"));

    CHECK_PARSE_THROW("func main; 0xabcdefgh end");
    CHECK_PARSE_THROW("func main; 0b010121 end");
    CHECK_PARSE_THROW("func main; 0o45678 end");

    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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
        )"));

    CHECK_PARSE_THROW("func main; 'aaaa' end");
    CHECK_PARSE_THROW("func main; '' end");
    CHECK_PARSE_THROW("func main; ''' end");
    CHECK_PARSE_THROW("func main; 43. end");
}

BOOST_AUTO_TEST_CASE(postfix_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

            aaa.bbb.ccc
            aaa().bbb.ccc
            aaa.bbb().ccc
            aaa.bbb.ccc()

            foo.bar(args)[3]
            foo[3].bar.baz(args)
            foo(hoge).bar[42]

            foo.awesome_member_func()
            foo.awesome_member_func(a, b)
            foo.awesome_member_func a
            foo.awesome_member_func a, b

            aaa''.bbb''.ccc''()
        end
        )"));

    CHECK_PARSE_THROW("func main; foo[42 end");
    CHECK_PARSE_THROW("func main; foo(42 end");
    CHECK_PARSE_THROW("func main; foo(42,a end");
    CHECK_PARSE_THROW("func main; foo(hoge.hu end");
}

BOOST_AUTO_TEST_CASE(type)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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
            expr : func(
                    int,
                    aaa
                    )
            expr : [func()]
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
            expr : func(int, aaa)
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

            (expr : int) + (expr : int)

            # Keyword corner cases
            expr : prochuga
        end
        )"));

    // Special callable types template
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func foo(pred : func)
        end

        func main
        end
    )"));

    CHECK_PARSE_THROW("func main; expr : proc() : int end # one element tuple is not allowed");
    CHECK_PARSE_THROW("func main; expr : proc() : int end # must not have ret type");
    CHECK_PARSE_THROW("func main; expr : T() end");
    CHECK_PARSE_THROW("func main; expr : [T](int) end");
    CHECK_PARSE_THROW("func main; expr : (T)(int) end");
    CHECK_PARSE_THROW("func main; expr : funchoge : int end");
}

BOOST_AUTO_TEST_CASE(primary_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            (1 + 2 * 3)
            (
                1 + 2 * 3
            )
            hogehoge # variable reference
            new int{42}
            new (int, int){42, 42}
            new (int,
             int){42,
                  42}
            new {int => string}{{1 => "aaa", 2 => "bbb"}}
        end
        )"));

    CHECK_PARSE_THROW("func main; (1 + 2; end");
    CHECK_PARSE_THROW("func main; new int{42; end");
}

BOOST_AUTO_TEST_CASE(unary_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            -42
            +42
            ~42
            !true
            -+~42
            !!true
        end
        )"));
}

BOOST_AUTO_TEST_CASE(cast_expression)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

            # corner case
            f.a as int
        end
        )"));

    {
        auto a = p.parse(R"(
                func main
                    expr
                        ashoge
                end
            )", "test_file");
        test_as_searcher s;
        dachs::ast::walk_topdown(a.root, s);
        BOOST_CHECK(!s.found);
    }
}

BOOST_AUTO_TEST_CASE(binary_expression)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            1 + 1
            1 - 1

            1 +
            1

            1 -
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
        )"));

    CHECK_PARSE_THROW("func main 1 == end");
    CHECK_PARSE_THROW("func main 1 + end");
    CHECK_PARSE_THROW("func main true && end");
    CHECK_PARSE_THROW("func main 1\n+\n1 end");
}

BOOST_AUTO_TEST_CASE(assignment_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            aaa = 42
            aaa, bbb = 42, 31
            aaa,
            bbb = 42,
                  31
            aaa, bbb = do_something()
            a : int = 42
            b : char = 'a'
            c : string = "aaa"
            d : float = 3.14
            e : bool = true
            f : uint = 42u
            t : (int, char, bool) = t
            t : (int, char, bool) = t
            t : (int, char, bool) = t
            (t : (int, char, bool))[0] = -42
            (t : (int, char, bool))[1] = 'b'
            (t : (int, char, bool))[2] = false

            aaa =
                42
            aaa, bbb =
                (ppp, qqq())
        end
        )"));
}

BOOST_AUTO_TEST_CASE(object_construct)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            new int{42}
            new int{
                42
               }
            new [int]{
                    [
                        1,
                        2,
                        3,
                    ]
                 }
            new {int => string}{{42 => "answer"}}
            new int
            new [int]
            new {int => string}

            new X
            new X(int)
            new X(int, char){1, 'a'}

            new X {|i| i + 1 }
            new X do
                ret 42 + 42
            end

            new X(T, U) {|i| i + 1 }
            new X(T, U) do
                ret 42 + 42
            end

            new X(T, U){1, 'a'} {|i| i + 1 }
            new X(T, U){1, 'a'} do
                ret 42 + 42
            end

            new X{}{ 42 }
        end
        )"));
}

BOOST_AUTO_TEST_CASE(lambda_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            l := -> a, b in a + b
            foo(
                -> a in foo a { a },
                -> a, var x in foo a,x { a + x },
            )
            l = ->
                a, b, c, d, e
            in
                foo(a, b, c, d, e)
            request(
                -> response in println("success: " + response as string),
                -> error in println("failure: " + error as string),
            )

            l := -> (a, b) in a + b
            foo(
                -> (a) in foo a { a },
                -> (a, var x) in foo a,x { a + x },
            )
            l = ->
                (a, b, c, d, e)
            in
                foo(a, b, c, d, e)

            (-> x in x * x)(2).println

            -> a, b do
                p := a + b
                println(p)
            end
            -> a, b do p := a + b; println(p); end
            ->
                a, b
            do
                p := a + b
                println(a + p)
            end
            request(
                -> response do
                    print("response: ")
                    println(response)
                end,
                -> error do
                    print("error: ")
                    println(error)
                end
            )

            -> (a, b) do
                p := a + b
                println(p)
            end
            -> (a, b) do p := a + b; println(p); end
            ->
                (a, b)
            do
                p := a + b
                println(a + p)
            end
            request(
                -> (response) do
                    print("response: ")
                    println(response)
                end,
                -> (error) do
                    print("error: ")
                    println(error)
                end
            )

            - -> 42()
            -> foo()().println
            l := -> 42
            (-> 42)().println
            -> foo a {l(a)}
            println(-> () in 42())
            -> () do
                println("hoge")
                println("fuga")
            end().println

            -> (a, b) : int in a + b
            -> (a, b) : klass(int, float)
            in
                a + b
            -> () : int in 42
            -> () : in in 42

            -> (a, b) : int do
                ret a + b
            end
            -> (a, b) : klass(int, float)
            do
                ret a + b
            end
            -> () : int do
                ret 42
            end
            -> () : do do
                ret 42
            end
        end
    )"));

    CHECK_PARSE_THROW(R"(
        # It can't contain statement
        func main
            foo bar {|i| j = i + 42}
        end
    )");

    // Corner cases
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            p := -> ()
            q := -> p()

            r := -> (p, q)       # tuple
            s := -> (p, q) in 42 # params
            t := -> () in (p, q)
        end
    )"));
}

BOOST_AUTO_TEST_CASE(variable_decl)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            a := 42
            var a := 42
            a := new int{42}
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
            var a, b := new int{32}, new char{'b'}
            var a, b := [] : [int], {} : {int => string}
            var a,
                b := [] : [int],
                     {} : {int => string}

            var a : int := 42
            var a :
                int := 42

            var_aaa # Corner case

            # Without initialization
            var a : int
            var a : char, var b : int
            var a : char,
            var b : int
            var a : char
            , var b : int
            var a : char
            , var b : int,
            var c : string
            var b
                : int
            var b :
                int

            var a' : int
            var a'' : int
        end
        )"));

    {
        auto a = p.parse(R"(
                func main
                    varhoge := 42
                end
            )", "test_file");
        test_var_searcher s;
        dachs::ast::walk_topdown(a.root, s);
        BOOST_CHECK(!s.found);
    }

    CHECK_PARSE_THROW("func main var a := b, end");
    CHECK_PARSE_THROW("func main var a,b : int end")
    CHECK_PARSE_THROW("func main var a, : int b end")
    CHECK_PARSE_THROW("func main var a : int, b end")
    CHECK_PARSE_THROW("func main var a : int, b : int end")
    CHECK_PARSE_THROW("func main a : int, var b : int end")
}

BOOST_AUTO_TEST_CASE(return_statement)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            ret
            ret 42
            ret 42, 'a', "bbb"
            ret 42,
                   'a',
                   "bbb"
            ret 42
                 , 'a'
                 , "bbb"

            # Keyword corner case
            returnhoge := 42
        end
        )"));
}

BOOST_AUTO_TEST_CASE(constant_decl)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        a := 42
        a := new int{42}
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
        a, b := new int{32}, new char{'b'}
        a, b := [] : [int], {} : {int => string}
        a,
        b := [] : [int],
                {} : {int => string}

        a : int := 42
        a :
        int := 42
        )"));

    CHECK_PARSE_THROW("a := b,");
}

BOOST_AUTO_TEST_CASE(if_statement)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

            # Keyword coner cases
            ifhoge := 42
            if thenhoge then
                elseifhoge
            elseif thenhoge then
                elsehoge
            else
                endhoge
            end

            unless aaaa
                expr
            end

            unless aaaa then
                expr
            end

            unless aaaa
                expr1
            else
                expr2
            end

            unless aaaa then
                expr1
            else
                expr2
            end

            unless aaaa then 42 else 52 end

            unless aaa
                expr
            elseif bbb
                expr
            elseif ccc
                expr
            end

            unless aaa
                expr
            elseif bbb
                expr
            elseif ccc
                expr
            else
                expr
            end

            unless aaa then
                expr
            elseif bbb then
                expr
            elseif ccc then
                expr
            else
                expr
            end

            unless aaa then bbb elseif bbb then expr elseif ccc then expr else ddd end

            unless aaaa then bbb end

            unless aaaa then bbb else ddd end

            # Keyword coner cases
            unlesshoge := 42
            unless thenhoge then
                elseifhoge
            elseif thenhoge then
                elsehoge
            else
                endhoge
            end
        end
        )"));

    CHECK_PARSE_THROW("func main if aaa then bbb else ccc end");
}

BOOST_AUTO_TEST_CASE(if_expr)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            (if true then 42 else 11 end) + 1
            (unless true then 42 else 11 end)
            (if true then 1 elseif false then 2 else 3 end)

            foo(
                if 11 == 11
                    i := 11
                    println(i)
                    i + 11
                else
                    moudameda()
                    0
                end
            )

            ret (
                unless 11 == 11
                    i := 11
                    println(i)
                    i + 11
                elseif 11 != 11
                    println("elseif")
                    i + 12
                else
                    moudameda()
                    0
                end
            )

            (if true then
                if true then
                    ret 52
                    42
                else
                    33
                end
            else
                unless foo()
                    ret 10
                    42
                elseif bar.baz
                    i + 10
                else
                    z + i
                end
            end)

            a :=
                if if true then true else false end
                    if if true then true else false end
                        true
                    else
                        else
                    end
                else
                    if if true then true else false end
                        true
                    elseif if true then true else false end
                        false
                    else
                        else
                    end
                end
        end
    )"));

    CHECK_PARSE_THROW(R"(
        func main
            (if true
                42
             end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            ret unless false then 42 end
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false then i := 42 else 32 end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false then 42 else i := 32 end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false
                42
            elseif false
                a := 12
            else
                32
            end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false
                42
             else
             end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false
             else
                42
             end)
        end
    )");

    CHECK_PARSE_THROW(R"(
        func main
            (unless false
                32
             elseif true
             else
                42
             end)
        end
    )");
}


BOOST_AUTO_TEST_CASE(switch_statement)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

            # Keyword corner cases
            casehoge := 42

            case whenhoge
            when thenhoge then
                elsehoge
            else
                endhoge
            end
        end
        )"));

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
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
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

            # Keyword corner cases
            casehoge := 42

            case
            when thenhoge then
                elsehoge
            else
                endhoge
            end
        end
        )"));

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
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            for a in arr
            end

            for a in arr
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

            for a in arr; akirameta; end

            # Keyword corner cases
            forhoge := 42
            for inhoge in dohoge
                endhoge
            end
        end
        )"));

    auto a = p.parse(R"(
            func main
                for varhoge in arr
                end
            end
        )", "test_file");
    test_var_searcher s;
    dachs::ast::walk_topdown(a.root, s);
    BOOST_CHECK(!s.found);
}

BOOST_AUTO_TEST_CASE(while_statement)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            for true
                moudameda
            end

            for true
            end

            for true
                moudameda
            end

            for true : bool
                madadameda
            end

            # Keyword corner cases
            forhoge := 42
            for dohoge
                endhoge
            end
        end
        )"));
}

BOOST_AUTO_TEST_CASE(function_invocation)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            foo()
            foo?()
            foo'()
            foo''()
            foo!()
            foo?'()
            foo?!()
            foo'!()
            foo?'!()
            foo?''()
        end
        )"));

    CHECK_PARSE_THROW("func main foo'?() end");
    CHECK_PARSE_THROW("func main foo!?() end");
    CHECK_PARSE_THROW("func main foo!'() end");
    CHECK_PARSE_THROW("func main foo!'?() end");
    CHECK_PARSE_THROW("func main foo!?'() end");
    CHECK_PARSE_THROW("func main foo'!?() end");
    CHECK_PARSE_THROW("func main foo?!'() end");
    CHECK_PARSE_THROW("func main foo'?!() end");
}

// TODO: Tests for postfix if statement

BOOST_AUTO_TEST_CASE(postfix_if)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            42 if true
            ret if true

            var v := 42
            v = 1 + 2 if true
            v += 42 if true
        end
        )"));

    // Parser parses 'ret if true' as postfix if statement and rest as an error
    CHECK_PARSE_THROW(R"(
        func main
            ret if true then 42 else -42
        end
        )");
}

BOOST_AUTO_TEST_CASE(let_stmt)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            let a := 42 in println(42)

            let
                a := 42
            in println(42)

            let
                a := 42
            in
            println(42)

            let
                a := 42
                b := 'a'
            in println(42)

            let a := 42; b := 'a' in println(42)

            let
                var a := 42
                var b := 42
            in begin
                for a < 50
                    println(a)
                    a += 1
                end
            end

            let
                a := 42
                b := 42
            in begin
                if true
                    println(42)
                end
                ret 99
            end

            let
                a := 42
                b := 42
            begin
                if true
                    println(42)
                end
                ret 99
            end

            result :=
                let
                    var a := 42
                    var b := 42
                begin
                    for a < 50
                        println(a)
                        a += 1
                    end
                    ret a
                end
        end
        )"));
}

BOOST_AUTO_TEST_CASE(do_stmt)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            do
            end

            do
                println(42)
            end

            do println(42) end
            do println(42); end
            do println(42); println(42) end

            do
                ret 42 if true

                if true
                    println(42)
                end

                case a
                when 42
                    42.println
                end
            end
        end
        )"));
}

BOOST_AUTO_TEST_CASE(do_block)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            foo(42) do
                blah
            end

            foo(42) do |i|
                blah
            end

            foo 42 do
                blah
            end

            foo 42 do |i|
                blah
            end

            foo 42,'a','b' do |i|
                blah
            end

            42.foo do
                blah
            end

            42.foo2(42) do
                blah
            end

            42.foo2 42 do
                blah
            end

            42.foo2 42,'a',b do
                blah
            end

            foo(42) do blah end

            foo(42) do |i| blah end

            42.foo do blah end

            42.foo2(42) do blah end

            42.foo2 42 do blah end

            # Edge case
            a.b + 42
            a.b(+42)
            a.b - 42
            a.b(-42)
        end
    )"));

    /* TODO: Not fixed yet
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func do_corner_case(p)
        end

        func do_corner_case(x, p)
        end

        func do_corner_case(x, y, p)
        end

        func main
            do_corner_case() do
            end

            42.do_corner_case do |i|
            end

            42.do_corner_case(42) do |i|
            end
        end
    )")); */
}

BOOST_AUTO_TEST_CASE(do_block2)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            foo(bar) { blah }
            foo(42) {
                blah
            }

            foo(bar) {|i| blah }
            foo(42) {|i|
                blah
            }

            foo bar { blah }
            foo 42 {
                blah
            }

            foo bar {|i| blah }
            foo 42 {|i|
                blah
            }

            foo 42,'a','b' {|i| blah }
            foo 42,'a','b' {|i|
                blah
            }

            42.foo { blah }
            42.foo {
                blah
            }

            42.foo2(42) { blah }
            42.foo2(42) {
                blah
            }

            42.foo2 foo { blah }
            42.foo2 42 {
                blah
            }

            42.foo2 42,'a',b { blah }
            42.foo2 42,'a',b {
                blah
            }

            foo bar {|i| -> 42 }

            42.expect to_be {|i| i % 2 == 0}
            42.should_be even?
        end
    )"));

    CHECK_PARSE_THROW(R"(
        # It can't contain statement
        func main
            foo bar {|i| j := i * 2 }
        end
    )");

    CHECK_PARSE_THROW(R"(
        # It can't contain statement
        func main
            baz.foo { ret 42 }
        end
    )");
}

BOOST_AUTO_TEST_CASE(clazz)
{
    // Instance variables
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
            var1
            var2
            var3 : int

          + var1
          + var2
          + var3 : int

          - var1
          - var2
          - var3 : int

            var1, var2, var3 : int
          + var1, var2, var3 : int
          - var1, var3 : int, var2
          + var3 : int, var1, var2

            var1
          , var2
          , var3 : int

          + var1
          , var2
          , var3 : int

          - var1
          , var3 : int
          , var2

        end
    )"));

    // Methods
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
            func method1
            end

          + func method2(a, b)
            end

          - func method2(a, b)
            end

            func +(a)
            end

            func +
            end
        end

        func main
        end
    )"));

    // Integration
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
        end

        class foo; end

        class foo
            var1
            var2 : float

            func method1(x)
                println(x)
            end

            func method2(x, y)
                method1(x+y)
            end
        end

        func main
        end
    )"));

    // Constructors
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
            init
            end

            init(a, b)
            end

            init(@aaa)
            end

            init(@bbb)
                @aaa = @bbb + 42
                println("ctor")
            end
        end
    )"));

    CHECK_PARSE_THROW(R"(
        # ' is not available for class name
        class foo'
        end
    )");

    // Instance variable access
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
            aaa, bbb

            init(@aaa, b)
                @bbb = @aaa + b
            end

            func foo(a, b)
                @aaa = 42
            end

            func bar(a, b)
                println(@aaa)
            end
        end
    )"));

    // Copiers
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class foo
            copy
                ret new foo
            end
        end
    )"));

    CHECK_PARSE_THROW(R"(
        class foo
            copy(a, b)
                ret new foo
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(import)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        import foo
        import foo2_
        import foo.bar
        import foo.bar.baz
        import foo.bar
    )"));

    CHECK_PARSE_THROW(R"(
        import foo.
    )");

    CHECK_PARSE_THROW(R"(
        import .foo
    )");

    CHECK_PARSE_THROW(R"(
        import foo..bar
    )");
}

BOOST_AUTO_TEST_CASE(static_array)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            new static_array(int)
            new static_array
        end
    )"));

    CHECK_PARSE_THROW(R"(
        class static_array
            a, b
        end

        func main
            new static_array(int, char)
        end
    )");
}

BOOST_AUTO_TEST_CASE(cast_function)
{
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        cast(i : int) : float
        end
        cast(i : int) : float; ret 3.14 end
    )"));

    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        class X
            cast : float
                ret 3.14
            end

            cast : char; ret 'a' end
            cast() : uint; ret 'a' end
        end
    )"));

    CHECK_PARSE_THROW(R"(
        cast(i : int)
        end
    )");

    CHECK_PARSE_THROW(R"(
        class X
            cast
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_not_degrade)
{
    // :foo was parsed as the return type of function 'main'
    BOOST_CHECK_NO_THROW(p.check_syntax(R"(
        func main
            :foo.println
        end
    )"));
}


BOOST_AUTO_TEST_SUITE_END()
