#define BOOST_TEST_MODULE LLVMCodegenExpressionTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"

#include <string>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/exception.hpp"

#include <boost/test/included/unit_test.hpp>

using namespace dachs::test;

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

            # Empty array literals
            a14 := [] : [int]
            a15 := [] : [float]
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

BOOST_AUTO_TEST_CASE(symbol)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(s)
        end

        func main
            :foo_hah

            println(:foo_hah)

            x := :foo_hah
            y := :yeah_hah

            (x != y).println
        end
    )");
}

BOOST_AUTO_TEST_CASE(string)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func each(str : string, pred)
            var i := 0
            for str[i] != '\0'
                pred(str[i])
                i += 1
            end
        end

        func len(str : string)
            var c := 0
            for str[c] != '\0'
                c += 1
            end
            ret c
        end

        func main(args)
            "abcdefghijklmnopqrstu".each(println)
            "abcdefghijklmnopqrstu".len.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main(args)
            var h := "hello"
            h2 := "hello"
            var i := 4
            println(h[4])
            println(h2[4])
            println(h[i])
            println(h2[i])
        end
    )");

}

BOOST_AUTO_TEST_SUITE_END()
