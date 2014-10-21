#define BOOST_TEST_MODULE ParserTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/parser/parser.hpp"
#include "dachs/exception.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/helper/util.hpp"
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

inline void validate(dachs::ast::ast const& a)
{
    auto root = a.root;
    dachs::ast::walk_topdown(root, test_visitor{});
}

inline void parse_and_validate(std::string const& code)
{
    validate(p.parse(code, "test_file"));
}

#define CHECK_PARSE_THROW(...) BOOST_CHECK_THROW(p.parse((__VA_ARGS__), "test_file"), dachs::parse_error)

BOOST_AUTO_TEST_SUITE(parser)

BOOST_AUTO_TEST_CASE(comment)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));

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
    BOOST_CHECK_NO_THROW(parse_and_validate("func main; end"));

    // general cases
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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

        func ..(l, r)
        end

        func ...(l, r)
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
    BOOST_CHECK_NO_THROW(parse_and_validate("proc p; end"));

    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));

    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        end
        )"));

    CHECK_PARSE_THROW("func main; foo[42 end");
    CHECK_PARSE_THROW("func main; foo(42 end");
    CHECK_PARSE_THROW("func main; foo(42,a end");
    CHECK_PARSE_THROW("func main; foo(hoge.hu end");
}

BOOST_AUTO_TEST_CASE(type)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
            expr : func : int
            expr : proc
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

            (expr : int) + (expr : int)

            # Keyword corner cases
            expr : prochuga
        end
        )"));

    CHECK_PARSE_THROW("func main; expr : proc() : int end # one element tuple is not allowed");
    CHECK_PARSE_THROW("func main; expr : proc() : int end # must not have ret type");
    CHECK_PARSE_THROW("func main; expr : func() end # must have ret type");
    CHECK_PARSE_THROW("func main; expr : T() end");
    CHECK_PARSE_THROW("func main; expr : [T](int) end");
    CHECK_PARSE_THROW("func main; expr : (T)(int) end");
    CHECK_PARSE_THROW("func main; expr : funchoge : int end");
}

BOOST_AUTO_TEST_CASE(primary_expr)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));

    CHECK_PARSE_THROW("func main; (1 + 2; end");
    CHECK_PARSE_THROW("func main; int{42; end");
}

BOOST_AUTO_TEST_CASE(unary_expr)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));

    CHECK_PARSE_THROW("func main 1 == end");
    CHECK_PARSE_THROW("func main 1 + end");
    CHECK_PARSE_THROW("func main true && end");
}

BOOST_AUTO_TEST_CASE(assignment_expr)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        end
        )"));
}

BOOST_AUTO_TEST_CASE(if_expr)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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

            # Corner cases
            (if thenhoge then elsehoge else 42)
            (ifhoge)

            (unless true then 42 else 24)
            hoge(unless true then 3.14 else 4.12)
            (unless true then
                42
            else
                24)
            (unless true then 42
             else 24)
            (unless unless then unless else unless) # 'unless' is a contextual keyword

            # Corner cases
            (unlesshoge)
            (unless thenhoge then elsehoge else 42)
        end
        )"));

    // it is parsed as if statement and it will fail
    CHECK_PARSE_THROW("func main if true then 42 else 24 end");
}

BOOST_AUTO_TEST_CASE(object_construct)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));
}

BOOST_AUTO_TEST_CASE(variable_decl)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        )"));

    CHECK_PARSE_THROW("a := b,");
}

BOOST_AUTO_TEST_CASE(if_statement)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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


BOOST_AUTO_TEST_CASE(switch_statement)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
        func main
            foo()
            foo?()
            foo'()
            foo!()
            foo?'()
            foo?!()
            foo'!()
            foo?'!()
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
                a := 42
                b := 'a'
            in if a == 4
                println(a)
            else
                println(b)
            end

            let
                var a := 42
                var b := 42
            in for a < 50
                println(a)
                a += 1
            end

            let
                a := 42
                b := 42
            in do
                if true
                    println(42)
                end
                ret 99
            end
        end
        )"));
}

BOOST_AUTO_TEST_CASE(do_stmt)
{
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
    BOOST_CHECK_NO_THROW(parse_and_validate(R"(
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
        end
        )"));
}

BOOST_AUTO_TEST_CASE(ast_nodes_node_illegality)
{
    dachs::syntax::parser p;
    for (auto const& d : {DACHS_ROOT_DIR "/test/assets/comprehensive", DACHS_ROOT_DIR "/test/assets/samples"}) {
        check_all_cases_in_directory(d, [&p](fs::path const& path){
                    std::cout << "testing " << path.c_str() << std::endl;
                    auto ast = p.parse(
                                *dachs::helper::read_file<std::string>(path.c_str())
                                , "test_file"
                           );
                    validate(ast);
                });
    }
}

BOOST_AUTO_TEST_CASE(comprehensive_cases)
{
    check_no_throw_in_all_cases_in_directory(DACHS_ROOT_DIR "/test/assets/comprehensive");
}

BOOST_AUTO_TEST_CASE(samples)
{
    check_no_throw_in_all_cases_in_directory(DACHS_ROOT_DIR "/test/assets/samples");
}

BOOST_AUTO_TEST_SUITE_END()
