#define BOOST_TEST_MODULE LLVMCodegenTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/exception.hpp"

#include <string>

#include <boost/test/included/unit_test.hpp>

static dachs::syntax::parser p;

#define CHECK_NO_THROW_CODEGEN_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            auto s = dachs::semantics::analyze_semantics(t); \
            dachs::codegen::llvmir::context c; \
            BOOST_CHECK_NO_THROW(dachs::codegen::llvmir::emit_llvm_ir(t, s, c)); \
        } while (false);

#define CHECK_THROW_CODEGEN_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            auto s = dachs::semantics::analyze_semantics(t); \
            dachs::codegen::llvmir::context c; \
            BOOST_CHECK_THROW(dachs::codegen::llvmir::emit_llvm_ir(t, s, c), dachs::code_generation_error); \
        } while (false);

BOOST_AUTO_TEST_SUITE(codegen_llvm)

BOOST_AUTO_TEST_CASE(function)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
        end

        func foo2(a)
        end

        func foo3(a, b)
        end

        func foo4(a : int) : int
            ret a + 42
        end

        func foo5(a)
            ret a
        end

        func foo6(var a, var b : float, var c)
            a += 42
            b += 42.0
            c = true
            ret a, b, c
        end

        func main
            foo()
            foo2(42)
            foo2('a')
            foo3(42, 'a')
            foo3(3.14, 42)
            foo5(foo4(foo5(13)))
            i := 42
            var b := false
            var t := foo6(i, 3.14, b)
            backward_ref_func()
        end

        func bar
        end

        func backward_ref_func
            bar()
            baz()
        end

        func baz
        end
        )");
}

BOOST_AUTO_TEST_CASE(variable)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := 42
            b := 'a'
            c := "aaa"
            d := 3.14
            e := true
            f := 42u
            var va := a
            var vb := b
            var vc := c
            var vd := d
            var ve := e
            var vf := f
            var vva := va
            var vvb := vb
            var vvc := vc
            var vvd := vd
            var vve := ve
            var vvf := vf

            va = 42
            vb = 'a'
            vc = "aaa"
            vd = 3.14
            ve = true
            vf = 42u

            va : int = 42
            vb : char = 'a'
            vc : string = "aaa"
            vd : float = 3.14
            ve : bool = true
            vf : uint = 42u

            var a2 : int
            var b2 : char
            var c2 : string
            var d2 : float
            var e2 : bool
            var f2 : uint

            a3, b3 := 42, 'a'
            var c3, d3 := 'a', "aaa"
            e3, var f3 := true, 42u
        end
    )");
}

BOOST_AUTO_TEST_CASE(print)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            print(42)
            print(3.14)
            print('a')
            print("aaa")
            print(true)
            print(42u)
            println(42)
            println(3.14)
            println('a')
            println("aaa")
            println(true)
            println(42u)

            a := 42
            b := 'a'
            c := "aaa"
            d := 3.14
            e := true
            f := 42u
            var va := a
            var vb := b
            var vc := c
            var vd := d
            var ve := e
            var vf := f

            print(a)
            print(b)
            print(c)
            print(d)
            print(e)
            print(f)
            print(va)
            print(vb)
            print(vc)
            print(vd)
            print(ve)
            print(vf)
        end
    )");
}

BOOST_AUTO_TEST_CASE(tuple)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            t := (1, 'a', true)
            var t2 := (1, 'a', true)
            var t3 : (int, char, bool)

            a := 42
            b := 'a'
            t4 := (a, b, true)
            var t5 := (a, b, true)

            t6 := foo2()
            var t7 := foo3()

            a2, var b2, c := t6
            var a3, b3, var c3 := foo()

            t2 = t
            t3 = t
            t7 = t

            t2 : (int, char, bool) = t
            t3 : (int, char, bool) = t
            t7 : (int, char, bool) = t

            t[0]
            t[1]
            t[2]

            bar(t)
            bar(t2)
            bar(t[0])
            bar(t2[1])

            t8 := (t2[0], t3[1], t7[2])
            var t9 := (t2[0], t3[1], t7[2])
            t9 = (t2[0], t3[1], t7[2])

            t9[0] = -42
            t9[1] = 'b'
            t9[2] = false

            t9[0] : int = -42
            t9[1] : char = 'b'
            t9[2] : bool = false

            (t9 : (int, char, bool))[0] = -42
            (t9 : (int, char, bool))[1] = 'b'
            (t9 : (int, char, bool))[2] = false

            t10 := ((1, 'a'), true)
            var t11 := ((1, 'a'), true)
            var t12 : ((int, char), bool)

            t13 := ((a, b), false)
            var t14 := ((a, b), false)

            t11 = t10
            t12 = t10

            t10[0][0]
            t10[0][1]
            bar(t10[0][0])
            bar(t10[0][1])

            p := t10[0][0]
            q := t10[0][1]
            t11[0][0] = 3
            t11[0][1]

            # Members
            var t15 := (('a', 'b'), true)
            println(t15.size)
            println(t15[0].size)
            println(t15[0].first)
            println(t15[0].second)
            t16 := (('a', 'b'), true)
            println(t16.size)
            println(t16[0].size)
            println(t16[0].first)
            println(t16[0].second)
        end

        func foo()
            ret 1, 'a', "bbb"
        end

        func foo2()
            t := (1, 'a', false)
            ret t
        end

        func foo3()
            var t := (1, 'c', false)
            ret t
        end

        func bar(a)
        end
    )");
}

BOOST_AUTO_TEST_CASE(array)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := [1, 2, 3]
            var a2 := [1, 2, 3]

            p := -1
            q := -2
            a4 := [p, q, -3]
            var a5 := [p, q, -3]

            a6 := foo()
            var a7 := foo2()
            a8 := foo2()
            var a9 := foo()

            a2 = a
            a5 = a
            a7 = a

            a2 = a2
            a5 = a2
            a7 = a2

            a[0]
            a[1]
            a[2]

            a2[0]
            a2[1]
            a2[2]

            var i := 2
            a[i]
            a2[i]

            bar(a)
            bar(a2)
            bar2(a)
            bar2(a2)

            a10     := [a2[0], a5[1], a7[2]]
            var a11 := [a2[0], a5[1], a7[2]]
            a2       = [a2[0], a5[1], a7[2]]

            a2[0] = -42
            a2[1] = a[0]
            a2[2] = a2[0]

            a12 := [[1, -1], [2, -2], [3, -3]]
            var a13 := [[1, 1], [2, -2], [3, -3]]

            a12[2][0]
            a13[2][0]

            a13[2][0] = 45
            a13[2][0] = a12[0][0]
            a13[2][0] = a13[0][0]

            s := a12[1][0]
            var t := a13[1][0]
            u := a13[1][0]
            var v := a12[1][0]

            bar(a12[0])
            bar(a13[0])
            bar2(a12[0])
            bar2(a13[0])

            baz(a12)
            baz(a13)
            baz2(a12)
            baz2(a13)

            # Members
            a.size
            a2.size
        end

        func foo()
            ret [1, 2, 3]
        end

        func foo2()
            var a := [1, 2, 3]
            ret a
        end

        func bar(a)
            ret a[0] + a[1]
        end

        func bar2(var a)
            ret a[0] + a[1]
        end

        func baz(a)
            ret a[0][0] + a[1][0]
        end

        func baz2(var a)
            ret a[0][0] + a[1][0]
        end
    )");
}

BOOST_AUTO_TEST_CASE(object_construction)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := new [char]{4u}
            var b := new [float]{4u}
            c := new [float]{4u, 3.14}
            var d := new [char]{4u, 'd'}
            var s := new [string]{4u, "aaa"}

            var e := new [uint]{32u}

            var i := 0u
            for i < e.size
                e[i] = i
                i += 1u
            end

            for i2 in e
                println(i2)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(binary_expression)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a)
        end

        func main
            foo(1 + 1)
            foo(1 - 1)
            foo(1 * 1)
            foo(1 / 1)
            foo(1 % 1)
            foo(1 < 1)
            foo(1 > 1)
            foo(1 & 1)
            foo(1 ^ 1)
            foo(1 | 1)
            foo(1 <= 1)
            foo(1 >= 1)
            foo(1 == 1)
            foo(1 != 1)
            foo(1 >> 1)
            foo(1 << 1)
            foo(true && true)
            foo(true || true)

            a := 42
            b := 53
            c := true
            d := false
            var p := 42
            var q := 53
            var r := true
            var s := false

            p += 42
            p -= 42
            p *= 42
            p /= 42
            p %= 42
            p &= 42
            p |= 42
            p <<= 4
            p >>= 4

            foo(a + b)
            foo(a - b)
            foo(a * b)
            foo(a / b)
            foo(a % b)
            foo(a < b)
            foo(a > b)
            foo(a & b)
            foo(a ^ b)
            foo(a | b)
            foo(a <= b)
            foo(a >= b)
            foo(a == b)
            foo(a != b)
            foo(a >> b)
            foo(a << b)
            foo(c && d)
            foo(c || d)

            foo(p + q)
            foo(p - q)
            foo(p * q)
            foo(p / q)
            foo(p % q)
            foo(p < q)
            foo(p > q)
            foo(p & q)
            foo(p ^ q)
            foo(p | q)
            foo(p <= q)
            foo(p >= q)
            foo(p == q)
            foo(p != q)
            foo(p >> q)
            foo(p << q)
            foo(r && s)
            foo(r || s)

            foo(a + a * a - a / a % a & a ^ a | a >> a << a)
            foo(a + (a * (a - a) / a) % a & a ^ a | (a >> a) << a)

            foo(p + p * p - p / p % p & p ^ p | p >> p << p)
            foo(p + (p * (p - p) / p) % p & p ^ p | (p >> p) << p)

            foo(1 < 3 || a > 5 && 6 == q || 8 != 9 || r)
            foo(1 < 3 || (b > 5) && (p == 7) || 8 != 9 || c)
        end
    )");
}

BOOST_AUTO_TEST_CASE(unary_expression)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a)
        end

        func main
            foo(-42)
            foo(+42)
            foo(~42)
            foo(!true)
            foo(-+~42)
            foo(!!true)
            foo(-~-~-~-~-~-~-~-~42)
            foo(!!!!!!!!!!!!!!!!!!true)

            var i := 42
            var b := true

            foo(-i)
            foo(+i)
            foo(~i)
            foo(!b)
            foo(-+~i)
            foo(!!b)
            foo(-~-~-~-~-~-~-~-~i)
            foo(!!!!!!!!!!!!!!!!!!b)
        end
    )");
}

BOOST_AUTO_TEST_CASE(if_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a)
        end

        func main
            (if true then 42 else 24)
            foo(if true then 3.14 else 4.12)
            (if true then
                42
            else
                24)
            var if := true
            (if true then 42
             else 24)
            (if if then if else if) # 'if' is a contextual keyword
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a)
        end

        func main
            (unless true then 42 else 24)
            foo(unless true then 3.14 else 4.12)
            (unless true then
                42
            else
                24)
            var unless := true
            (unless true then 42
             else 24)
        end
    )");
}

BOOST_AUTO_TEST_CASE(typed_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var i : int := 42 : int
            i2 := 42 : int
            var j := (i : int) + (i2 : int)

            # issue 4
            # (j : int) += i : int
        end
    )");
}

BOOST_AUTO_TEST_CASE(cast_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            d := 3.14
            var d2 := 3.14

            i := d as int
            i2 := d2 as int
            i3 := 3.14 as int
            var i4 := d as int
            var i5 := d2 as int
            var i6 := 3.14 as int
        end
    )");
}

BOOST_AUTO_TEST_CASE(return_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
            ret
        end

        func foo2
        end

        func foo3
            ret 42
        end

        func foo4
            var i := 42
            ret i
        end

        func foo5
            var i := 42
            ret i, true
        end

        func foo6
            ret 42, true
        end

        func main
            foo()
            foo2()
            var i1 := foo3()
            var i2 := foo4()
            var t := foo5()
            var t2 := foo6()
            var i3, var b1 := foo5()
            var i4, var b2 := foo6()
        end
    )");
}

BOOST_AUTO_TEST_CASE(if_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func pred
            ret true
        end

        func dummy_pred(b)
            ret true if b
            ret false unless b
            ret true
        end

        func dummy2
            ret if true
            ret unless false
        end

        func dummy3
            ret (if true then 42 else -42)
        end

        func main

            if true
                println("hoge")
            end

            if pred()
                println("huga")
            else
                i := 42
            end

            if false
                i := 42
                var j := 42
            elseif pred()
                println(42)
            end

            if false
                i := 42
                var j := 42
            elseif pred()
                println(42)
            else
                var b := false
                println(b)
            end

            print(42) if false
            print(42) if pred()
            var i := 42
            i += 42 if dummy_pred(true)

            unless true
                println("hoge")
            end

            unless pred()
                println("huga")
            else
                i := 42
            end

            unless false
                i := 42
                var j := 42
            elseif pred()
                println(42)
            end

            unless false
                i := 42
                var j := 42
            elseif pred()
                println(42)
            else
                var b := false
                println(b)
            end

            print(42) unless false
            print(42) unless pred()
            i += 42 unless dummy_pred(true)

            dummy2()
            dummy3()
        end
    )");
}

BOOST_AUTO_TEST_CASE(switch_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func dummy(a)
            println(a)
        end

        func main
            i := 42
            case i
            when 42
                println(i)
            when 0
            when 0, 1, 2
                i + 42
            end

            case i
            when 42
                println(i)
            when 0
                dummy(i + 42)
            else
                ;
            end

            var j := i
            case j
            when 42
                println(j)
            when 0
                j + 42
            end

            case j
            when 42
                println(j)
            when 0, 1, 2
                dummy(j + 42)
            else
                ;
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(case_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func dummy(a)
            println(a)
        end

        func main
            var a := 32

            case
            when true
                println("aaa")
            when a == 1
                ;
            when a == -32
                dummy(a + 32)
            else
                a : int
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(for_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func make_arr
            ret ['a', 'b', 'c']
        end

        func make_arr2
            var a := ['a', 'b', 'c']
            ret a
        end

        func main
            for i in [1, 2, 3, 4, 5]
                print(i)
            end

            a := [1, 2, 3, 4, 5]
            for i in a
                print(i)
            end

            var a2 := [1, 2, 3, 4, 5]
            for i in a2
                print(i)
            end

            for i in make_arr()
                print(i)
            end

            for i in make_arr2()
                print(i)
            end

            for var i in [1, 2, 3, 4, 5]
                print(i)
            end

            for var i in a
                print(i)
            end

            for var i in a2
                print(i)
            end

            for var i in make_arr()
                print(i)
            end

            for var i in make_arr2()
                print(i)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(while_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func dummy(a)
            println(a)
        end

        func main
            var a := 32

            for false
            end

            for true
                dummy(a)
            end

            var i := 0
            for i < 10
                i += 1
                println(i)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(postfix_if_statement)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            ret if x
        end

        func foo2(x)
            ret if x
            ret
        end

        func abs(x)
            ret -x if x as float < 0.0
            ret x
        end

        func main
            foo(true)
            foo2(false)
            abs(-3).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            ret unless x
        end

        func foo2(x)
            ret unless x
            ret
        end

        func abs(x)
            ret -x unless x as float > 0.0
            ret x
        end

        func main
            foo(true)
            foo2(false)
            abs(-3).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            if x
                ret if if x then !x else x
            end
        end

        func foo2(x)
            unless x
                ret unless unless x then !x else x
            end
        end

        func main
            foo(true)
            foo2(true)
        end
    )");
}

BOOST_AUTO_TEST_CASE(function_variable)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(i : int)
        end

        func generic_foo(i)
        end

        func higher_order_foo(f, a)
            f(a)
        end

        func main
            f := foo
            var f2 := foo
            f(42)
            f2(42)

            gf := generic_foo
            var gf2 := generic_foo
            gf('a')
            gf2("aaa")

            higher_order_foo(f, 42)
            higher_order_foo(f2, 42)
            higher_order_foo(gf, 'a')
            higher_order_foo(gf2, "aaa")

            # built-in function
            bf := println
            var bf2 := println
            bf("aaa")
            bf2(42u)

            # undetermined generic function variable
            unused := generic_foo
            var unused2 := generic_foo

            # call generic function with overload resolution
            gf3 := generic_foo
            gf3(42)
            gf3(3.14)

            var gf4 := generic_foo
            gf4(42)
            gf4(3.14)
        end
    )");
}

BOOST_AUTO_TEST_CASE(ufcs)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(i : int)
            ret i
        end

        func bar(i)
            ret i
        end

        func baz(i, j)
            ret j
        end

        func main
            var i := 42

            42.foo
            i.foo
            42.foo()
            i.foo()
            42.bar
            i.bar
            42.bar()
            i.bar()

            3.14.bar
            3.14.bar()

            42.baz(3)
            42.baz(3.14)
            i.baz(3)
            i.baz(3.14)

            42.foo.bar
            42.foo.bar()
            42.foo().bar
            42.foo().bar()
            42.foo.baz(3.14)
            42.foo().baz(3.14)
        end
    )");
}

BOOST_AUTO_TEST_CASE(let_stmt)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
            let
                a := 42
            in ret a
        end

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

            let
                var a,b := 42,'b'
            in println(42)

            let a := 42; b := 'a' in println(42)

            let
                var a := 42
                b := 'a'
            in if a == 4
                println(a)
            else
                println(b)
            end

            let
                a, var b := 42, 'a'
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

            foo().println

            let
                a := 42
            in case a
            when 42
                println("42")
            end

            let var a := 42 in
            a = 21

            let var a := 42 in
            b := a
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_stmt)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func p(a, b)
            println(a + b)
        end

        func main
            a := 42

            do
            end

            do end

            do
                a := 42
                println(a)
            end

            do
                do
                    do
                    end
                end
            end

            do
                a := 42
                b := 42
                a.p b
                b.p a
            end

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

            let
                a := 42
                b := 53
            in do
                println(a)
                println(a + b)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_block)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func ufcs_do_no_arg(a, p)
            p()
        end

        func ufcs_do_arg1(a, p)
            p(a)
        end

        func ufcs_do_no_arg2(a, var p)
            p()
        end

        func ufcs_do_arg12(a, var p)
            p(a)
        end

        func ufcs2_do_arg1(a, b, p)
            p(a + b)
        end

        func ufcs2_do_arg12(a, b, var p)
            p(a + b)
        end

        func ufcs2_do_no_arg(a, b, p)
            p()
        end

        func ufcs2_do_no_arg2(a, b, p)
            p()
        end

        func ufcs2_do_arg2(a, b, p)
            p(a, b)
        end

        func ufcs2_do_arg22(a, b, var p)
            p(a, b)
        end

        func main
            42.ufcs_do_no_arg do
                println("ufcs no arg")
            end

            42.ufcs_do_arg1 do |i|
                j := i * i
                println(j)
            end

            42.ufcs_do_arg1 do |var i|
                i = i * i
                println(i)
            end

            42.ufcs_do_no_arg2 do
                println("ufcs no arg")
            end

            42.ufcs_do_arg12 do |i|
                j := i * i
                println(j)
            end

            42.ufcs_do_arg12 do |var i|
                i = i * i
                println(i)
            end

            ufcs_do_no_arg(42) do
                println("ufcs no arg")
            end

            ufcs_do_no_arg 42 do
                println("ufcs no arg")
            end

            ufcs_do_no_arg2(42) do
                println("ufcs no arg")
            end

            42.ufcs2_do_no_arg 42 do
                println("ufcs no arg")
            end

            42.ufcs2_do_arg1 42 do |i|
                println(i)
            end

            42.ufcs2_do_arg2 42 do |i, j|
                println(i + j)
            end

            42.ufcs2_do_no_arg2 42 do
                println("ufcs no arg")
            end

            42.ufcs2_do_arg12 42 do |i|
                println(i)
            end

            42.ufcs2_do_arg22 42 do |i, j|
                println(i + j)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func ufcs_do_no_arg(a, p)
            p()
        end

        func ufcs_do_arg1(a, p)
            p(a)
        end

        func ufcs_do_no_arg2(a, var p)
            p()
        end

        func ufcs_do_arg12(a, var p)
            p(a)
        end

        func ufcs2_do_arg1(a, b, p)
            p(a + b)
        end

        func ufcs2_do_arg12(a, b, var p)
            p(a + b)
        end

        func ufcs2_do_no_arg(a, b, p)
            p()
        end

        func ufcs2_do_no_arg2(a, b, p)
            p()
        end

        func ufcs2_do_arg2(a, b, p)
            p(a, b)
        end

        func ufcs2_do_arg22(a, b, var p)
            p(a, b)
        end

        func apply(arg, f)
            ret f(arg)
        end

        func main
            foo := 42

            42.ufcs_do_arg1 {|i| i as float + 3.14 }

            42.ufcs_do_arg1 {|var i| println(i * i) }

            42.ufcs_do_arg12 {|_| "hoge" }

            42.ufcs_do_arg12 {|var i| i * i}

            42.ufcs_do_no_arg { println("ufcs no arg") }

            42.ufcs_do_no_arg2 { println("ufcs no arg") }

            ufcs_do_no_arg(foo) { println("ufcs no arg") }

            ufcs_do_no_arg foo { println("ufcs no arg") }

            ufcs_do_no_arg2(foo) { println("ufcs no arg") }

            42.ufcs2_do_no_arg foo { println("ufcs no arg") }

            42.ufcs2_do_arg1 foo {|i| println(i) }

            42.ufcs2_do_arg2 foo {|i, j| println(i + j) }

            42.ufcs2_do_no_arg2 foo { println("ufcs no arg") }

            42.ufcs2_do_arg12 foo {|i| println(i) }

            42.ufcs2_do_arg22 foo {|i, j| println(i + j) }

            42.apply {|i| i * i} == 42 * 42

            42.0.apply {|_| 3.14} == 3.14
        end
    )");

    // Unused blocks
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func nothing(a, p)
        end

        func nothing(p)
        end

        func main
            42.nothing do |i|
                println(i)
            end

            42.nothing do
                println("non template")
            end

            nothing(42) do |i|
                println(i)
            end

            nothing(42) do
                println("non template")
            end

            nothing() do |i|
                println(i)
            end

            nothing() do
                println("non template")
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func each(a, b)
            for e in a
                b(e)
            end
        end

        func main
            [1, 2, 3].each do |i|
                println(i)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_block_with_captures)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func apply(a, b, p)
            p(a, b)
        end

        func output(a, b, p)
            println(a + b + p())
        end

        func plus(a, b, p)
            ret a + b + p()
        end

        func verbose(a, p)
            println("start")
            p(a)
            println("end")
        end

        func verbose2(a, p)
            println("start")
            p()
            println("end")
        end

        func verbose(p)
            println("start")
            p()
            println("end")
        end

        func main
            # func_invocation, function template
            do
                a := 42
                var b:= 21

                42.apply a do |i, j|
                    println(i + j + a)
                end

                b.apply a do |i, j|
                    println(i + j + a + b)
                end

                a.apply 42 do |i, j|
                    i.apply j do |p, q|
                        println(a + b + i + j + p + q)
                    end
                end
            end

            # func_invocation, function template
            do
                a := 42
                var b:= 21

                42.output a do
                    ret a
                end

                b.output a do
                    ret a + b
                end

                a.apply 42 do |i, j|
                    i.output j do
                        ret a + b + i + j
                    end
                end

                a.plus 42 do
                    ret 42.plus b do
                        ret a + b
                    end
                end.println

                verbose() do
                    verbose() do
                        verbose() do
                            verbose() do
                                "foo!".println
                            end
                        end
                    end
                end
            end

            # ufcs_invocation, function template
            do
                a := 42
                var b:= 21

                42.verbose do |i|
                    println(a + b + i)
                end

                a.verbose do |i|
                    println(a + b + i)
                end

                b.verbose do |i|
                    println(a + b + i)
                end
            end

            # ufcs_invocation, non function template
            do
                a := 42
                var b:= 21

                42.verbose2 do
                    println(a + b)
                end

                a.verbose2 do
                    println(a + b)
                end

                b.verbose2 do
                    println(a + b)
                end
            end
        end
    )");

    // do-end block for lambda
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a, p)
            p(a * 2) do |i|
                println(i)
            end
        end

        func main
            42.foo do |i, p|
                p(i)
            end
        end
    )");

    // Generic lambda overload
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(p)
            p(42)
            p(42.0)
            p('a')
            p(true)
        end

        func main
            foo() do |x|
                println(x)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            (-> -> 42)()().println
            (-> -> println(42))()()

            a := 42
            (-> -> a)()().println

            var b := 42
            (-> -> b)()().println
        end
    )")
}

BOOST_AUTO_TEST_CASE(unit_type)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
        end

        func foo2 : ()
            ret
        end

        func foo3 : ()
            ret ()
        end

        func bar(a)
        end

        func bar2(a : ())
        end

        func bar3(a)
            bar(a())
        end

        func main
            bar(foo())
            bar(foo2())
            bar(foo3())
            bar2(foo())
            bar2(foo2())
            bar2(foo3())

            bar3() do
                ret foo()
            end

            bar3(foo)
        end
    )");
}

BOOST_AUTO_TEST_CASE(lambda_expression)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func bind1st(x, f)
            ret -> y in f(x, y)
        end

        func plus(x, y)
            ret x + y
        end

        func returns_42(x)
            ret x() as int == 42
        end

        func fourty_two
            ret 42
        end

        func main
            y := 42

            f := -> x in x + 42
            f(21).println

            g := -> x in -f(x)
            f(g((g(21) + f(21)))).println

            p := -> ()
            q := -> p()

            r := -> 42

            returns_42(fourty_two).println
            returns_42(){42}.println
            returns_42(r).println

            s := bind1st(21, plus)
            s(11).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            ret x * 2
        end

        func main
            y := 42
            f := -> x do
                z := x + 42
                println(z)
            end
            f(21)

            -> x do
                f(x)
            end(21)

            h := -> do
                ret 42
            end

            i := -> do
                println(h())
            end
            i()

            # j := -> x, y do
            #     ret h() * foo(21) * x * y * 21
            # end(2, 3).println
        end
    )");

    // Lambda is generic
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            double := -> x in x * x

            double(42).println
            double(3.14).println
        end
    )");

    // Access to captured values in different basic block
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            x()
        end

        func foo2(x)
            ret x(42)
        end

        func main
            var a := 42
            foo(-> println(42))

            b := 3.14
            foo(-> println(b))

            foo2(-> x in x + a).println
            foo2(-> x in x as float + b).println
        end
    )");

    // Returned lambdas
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
            ret -> 42
        end

        func foo2
            ret -> x in x * x
        end

        func foo3(a)
            ret -> x in a * x
        end

        func main
            foo()() == 42
            foo2()(42) == 42 * 42
            foo3(10)(32) == 10 * 32
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func curry3(f)
            ret -> x in
                    -> y in
                        -> z in
                            f(x, y, z)
        end

        func mult3(x, y, z)
            ret x * y * z
        end

        func main
            m := curry3(mult3)
            m(1)(2)(3) == mult3(1, 2, 3)
        end
    )");
}
BOOST_AUTO_TEST_CASE(return_stmt_in_the_middle_of_basic_block)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            ret 0
            println(42)
        end
    )");
}

BOOST_AUTO_TEST_CASE(some_samples)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            print("Hello, Dachs!\n")
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func fib(n)
            case n
            when 0, 1
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n : float) : float
            ret (if n > 0.0 then n else -n)
        end

        func sqrt(x : float) : float
            var z := x
            var p := 0.0
            for abs(p-z) > 0.00001
                p, z = z, z-(z*z-x)/(2.0*z)
            end
            ret z
        end

        func main
            print(sqrt(10.0))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n)
            ret (if n > 0.0 then n else -n)
        end

        func sqrt'(p, z, x)
            ret z if abs(p-z) < 0.00001
            ret sqrt'(z, z-(z*z-x)/(2.0*z), x)
        end

        func sqrt(x)
            ret sqrt'(0.0, x, x)
        end

        func main
            print(sqrt(10.0))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main()
            print(fib(10))
        end

        func fib(n)
            if n <= 1
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func fib(n)
            case n
            when 0, 1
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func map(var a, p)
            var i := 0u
            for i < a.size
                a[i] = p(a[i])
                i += 1u
            end

            ret a
        end

        func square(x)
            ret x * x
        end

        func main
            a := [1, 2, 3, 4, 5]
            a2 := map(a, square)
            for e in a2
                println(e)
            end

            var a3 := [1.1, 2.2, 3.3, 4.4, 5.5]
            var a4 := map(a3, square)
            for e in a4
                println(e)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func map(var a, p)
            var i := 0u
            for i < a.size
                a[i] = p(a[i])
                i += 1u
            end

            ret a
        end

        func each(a, p)
            for e in a
                p(e)
            end
        end

        func square(x)
            ret x * x
        end

        func main
            [1, 2, 3, 4, 5].map(square).each println
            [1.1, 2.2, 3.3, 4.4, 5.5].map(square).each println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func each(a, p)
            for e in a
                p(e)
            end
        end

        func main
            [1, 2, 3, 4].each do |i|
                println(i)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            f := -> x in x + 1
            f2 := -> 42
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
