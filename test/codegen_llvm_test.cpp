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
            dachs::codegen::llvmir::emit_llvm_ir(t, s, c); \
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
            return a + 42
        end

        func foo5(a)
            return a
        end

        func foo6(var a, var b : float, var c)
            a += 42
            b += 42.0
            c = true
            return a, b, c
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
            return 1, 'a', "bbb"
        end

        func foo2()
            t := (1, 'a', false)
            return t
        end

        func foo3()
            var t := (1, 'c', false)
            return t
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
            return [1, 2, 3]
        end

        func foo2()
            var a := [1, 2, 3]
            return a
        end

        func bar(a)
            return a[0] + a[1]
        end

        func bar2(var a)
            return a[0] + a[1]
        end

        func baz(a)
            return a[0][0] + a[1][0]
        end

        func baz2(var a)
            return a[0][0] + a[1][0]
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
            return
        end

        func foo2
        end

        func foo3
            return 42
        end

        func foo4
            var i := 42
            return i
        end

        func foo5
            var i := 42
            return i, true
        end

        func foo6
            return 42, true
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
            return true
        end

        func dummy_pred(b)
            return true if b
            return false unless b
            return true
        end

        func dummy2
            return if true
            return unless false
        end

        func dummy3
            return (if true then 42 else -42)
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
                return 1
            else
                return fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n : float) : float
            return (if n > 0.0 then n else -n)
        end

        func sqrt(x : float) : float
            var z := x
            var p := 0.0
            for abs(p-z) > 0.00001
                p, z = z, z-(z*z-x)/(2.0*z)
            end
            return z
        end

        func main
            print(sqrt(10.0))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n)
            return (if n > 0.0 then n else -n)
        end

        func sqrt'(p, z, x)
            return z if abs(p-z) < 0.00001
            return sqrt'(z, z-(z*z-x)/(2.0*z), x)
        end

        func sqrt(x)
            return sqrt'(0.0, x, x)
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
                return 1
            else
                return fib(n-1) + fib(n-2)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func fib(n)
            case n
            when 0, 1
                return 1
            else
                return fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
