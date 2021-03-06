#define BOOST_TEST_MODULE LLVMCodegenExpressionTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"
#include "./codegen_test_helper.hpp"

using namespace dachs::test;

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
            g := :foo
            var va := a
            var vb := b
            var vc := c
            var vd := d
            var ve := e
            var vf := f
            var vg := g
            var vva := va
            var vvb := vb
            var vvc := vc
            var vvd := vd
            var vve := ve
            var vvf := vf
            var vvg := vg

            va = 42
            vb = 'a'
            vc = "aaa"
            vd = 3.14
            ve = true
            vf = 42u
            vg = :bar

            va : int = 42
            vb : char = 'a'
            vc : string = "aaa"
            vd : float = 3.14
            ve : bool = true
            vf : uint = 42u
            vg : symbol = :poyo

            var a2 : int
            var b2 : char
            var d2 : float
            var e2 : bool
            var f2 : uint

            a3, b3 := 42, 'a'
            var c3, d3 := 'a', "aaa"
            e3, var f3 := true, 42u
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func main
            var x1 : X(int)
            var x2 : X(X(int))
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
            a16 := [3.14, 3.12] : [float]
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

BOOST_AUTO_TEST_CASE(aggregates)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func print_1_0(a)
            a[1][0].println
        end
        func main
            begin
                var a := [1]
                var t := ('a', a)
                print_1_0(t)
                c := 1
            end

            begin
                var a := [1]
                t := ('a', a)
                print_1_0(t)
                var i := 10
                for i > 0
                    i -= 1
                end
            end

            begin
                var t := (-1, -2)
                var a := [t, t]
                print_1_0(a)
            end

            begin
                var t := (-1, -2)
                a := [t, t]
                print_1_0(a)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(object_construction)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
        end

        class Y
            a
        end

        class Z
            a

            init(b)
                @a := b(42)
            end
        end

        func main
            a := new [char]{4u, '\0'}
            var b := new [float]{4u, 0.0}
            c := new [float]{4u, 3.14}
            var d := new [char]{4u, 'd'}
            var s := new [string]{4u, "aaa"}

            var e := new [uint]{32u, 0u}
            var x := new [X]{4u, new X}

            var i := 0u
            for i < e.size
                e[i] = i
                i += 1u
            end

            for i2 in e
                println(i2)
            end

            var cc := 'a'
            aa := new [char]{4u, cc}

            xx := new X
            bb := new [X]{4u, xx}
            y := new Y{42}
            yy := new Y(int)

            z := new Z {|j| j * j }
            zz := new Z do |i|
                    r := z.a + i
                    ret r * r
                end

            y2 := new Y{-42}
            yy2 := new Y do |i, j|
                ret y2.a * (i + j)
            end
            yy2.a(5, 1).println
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
            (if true then 42 else 24 end)
            foo(if true then 3.14 else 4.12 end)
            (if true then
                42
            else
                24 end)
            var if := true
            (if true then 42
             else 24 end)
            (if if then if else if end) # 'if' is a contextual keyword
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a)
        end

        func main
            (unless true then 42 else 24 end)
            foo(unless true then 3.14 else 4.12 end)
            (unless true then
                42
            else
                24 end)
            var unless := true
            (unless true then 42
             else 24 end)
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(x)
            ret if (x as int) < 0 then
                -x
            else
                x
            end
        end

        func foo(i)
            ret if i < 10 then
                ret 42
                0
            else
                unless i > 10
                    1
                else
                    ret 0
                    -1
                end
            end
        end

        func main
            abs(-1.0).println

            i :=
                if abs(30) > 0
                    k, l := 42, 21
                    if k < l
                        k
                    else
                        l
                    end
                else
                    k, l := 'a', 'b'
                    if 'a' > (0 as char)
                        k as int
                    else
                        l as int
                    end
                end

            foo(9)
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            ret if true then
                ret 0
                a := 10
                0
            elseif false
                ret 0
                println('f') if true
                0
            else
                ret 0
                0
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            ret unless x == 0 as typeof(x) then
                ret 1.0
                3.14
            else
                ret 2.0
                2.15
            end
        end

        func main
            foo(0.0).println
            ret if true then
                ret 'p'
                'a'
            elseif false
                ret 'q'
                'b'
            else
                ret 'r'
                'c'
            end
        end
    )");


    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func main
            var p := new pointer(X(int)){2u}

            p[0u] =
                if false
                    new X{42}
                else
                    i := 42
                    if false
                        new X{i}
                    else
                        new X{i - 31}
                    end
                end

            p[1u] = new X{
                    if true
                        if false
                            12
                        else
                            43
                        end
                    else
                        -1
                    end
                }

            i := 31

            (
                if i % 2 == 0
                    p[0u]
                else
                    p[1u]
                end
            ).a.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(case_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
        end

        func abs(i)
            ret case
                when i < 0
                    -i
                else
                    i
                end
        end

        func main
            foo(case
                when true
                    3.14
                when false
                    1.14
                else
                    2.14
                end
            )

            abs(-10).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := 0

            ret case
                when a > 0
                    ret 0
                    b := 10
                    a + b
                when a < 0
                    ret 0
                    b := 11.0
                    a + (b as int)
                else
                    0
                end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func abs(var x)
            ret case
                when x.a < 0
                    x.a = -x.a
                    x
                else
                    x
                end
        end

        func main
            print('X')
            abs(new X{-10}).a.println

            var p := new pointer(X(int)){2u}
            p[0u] = new X{11}
            p[1u] = new X{-100}

            i := 11

            x := case
            when p[0u].a % 2 == 0
                p[0u]
            when p[1u].a % 2 == 0
                p[1u]
            else
                new X{0}
            end

            print('X')
            x.abs.a.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(switch_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(x)
            ret x
        end

        func abs(i)
            ret case i % 2
                when 0, 1
                    -i
                else
                    i
                end
        end

        func main
            b := true

            i := foo(case b
                when true
                    3.14
                when false
                    1.14
                else
                    2.14
                end
            ) as int

            abs(i).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := 0

            ret case a
                when 0, 1, 2
                    ret 0
                    b := 10
                    a + b
                when 3, 4, 5
                    ret 0
                    b := 11.0
                    a + (b as int)
                else
                    0
                end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func abs(var x)
            ret case x.a % 2
                when 0
                    x.a = -x.a
                    x
                else
                    x
                end
        end

        func main
            print('X')
            abs(new X{-10}).a.println

            var p := new pointer(X(int)){2u}
            p[0u] = new X{11}
            p[1u] = new X{-100}

            i := 11

            x := case p[0u].a % 2
                when 0
                    p[0u]
                when 1, 2
                    p[1u]
                else
                    new X{0}
                end

            print('X')
            x.abs.a.println
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

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            new pointer(int){3u} as uint
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

        func main
            "abcdefghijklmnopqrstu".each(println)
            "abcdefghijklmnopqrstu".len.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
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

BOOST_AUTO_TEST_CASE(lambda)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func compose(f, g)
            ret -> x in f(g(x))
        end

        func square(x)
            print("square: "); println(x)
            ret x * x
        end

        func twice(x)
            print("twice: "); println(x)
            ret x + x
        end

        func main
            f := compose(compose(square, twice), twice)
            f(10).println
        end
    )");

    // Block expression bug
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            (-> i in
                begin
                    j := 42
                    println(i + j)
                end)(10)
        end
    )");

}

BOOST_AUTO_TEST_CASE(address_of)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
        end

        func main
            __builtin_address_of([1, 2, 3])
            __builtin_address_of((1, 'a', 1.1))
            __builtin_address_of(new X)
            __builtin_address_of(-> println("foo"))
            __builtin_address_of("aaa")
        end
    )");

    CHECK_THROW_CODEGEN_ERROR(R"(
        func main
            var i := 42
            __builtin_address_of(i)
        end
    )");

    CHECK_THROW_CODEGEN_ERROR(R"(
        func main
            __builtin_address_of(3.14)
        end
    )");
}

BOOST_AUTO_TEST_CASE(realloc)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var p := new pointer(int){8u}
            (p as uint).println

            p = p.__builtin_realloc(10u)
            (p as uint).println

            p = p.__builtin_realloc(0u)
            (p as uint).println

            var i := 3u
            p = p.__builtin_realloc(i)
            (p as uint).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            p := new pointer(int){8u}
            p.__builtin_realloc(10u)
            p.__builtin_realloc(0u)
            var i := 3u
            p.__builtin_realloc(i)
        end
    )");
}

BOOST_AUTO_TEST_CASE(gc_builtins)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            __builtin_gc_disabled?().println
            __builtin_disable_gc()
            __builtin_gc_disabled?().println
            __builtin_enable_gc()
        end
    )");
}

BOOST_AUTO_TEST_CASE(gen_symbol)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var p := new pointer(char){4u}
            p[0] = 'd'
            p[1] = 'o'
            p[2] = 'g'
            p[3] = '\0'
            sym := __builtin_gen_symbol(p, 3u)
            sym == :dog
        end
    )");
}

BOOST_AUTO_TEST_CASE(let_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
            ret let
                a := 42
            in a
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
            in if a == 4 then
                println(a)
            else
                println(b)
            end

            let
                a, var b := 42, 'a'
            in if a == 4 then a else (b as int) end

            let
                var a := 42
                var b := 42
            in begin
                for a < 50
                    println(a)
                    a += 1
                end
                ()
            end

            foo().println

            let
                a := 42
            in begin
                case a
                when 42
                    println("42")
                end
                ()
            end

            let var a := 42 in begin a = 21; a end

            let var a := 42 in begin b := a; b end

            let
                var a := 42
                var b := 42
            in begin
                for a < 50
                    println(a)
                    a += 1
                end
                ()
            end

            let
                a := 42
            in begin
                case a
                when 42
                    println("42")
                end
                ()
            end

            ret let
                a := 42
            in begin
                b := 42
                case a
                when 42
                    1
                when -42
                    2
                else
                    b
                end
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(begin_expr)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo
            ret begin
                ret 42
                a := 42
                a + 10
            end
        end

        class X
            a
        end

        func foo(var x)
            ret begin
                ret x.a + 20
                x.a = 42
                x.a + 10
            end
        end

        func main
            a := begin
                i := 10
                var j := i + 1
                i + j
            end

            println(
                begin
                    i,j := a + 10, a - 10
                    i * 10
                end
            )

            foo()
            foo(new X{10})
        end
    )");
}

BOOST_AUTO_TEST_CASE(getchar_builtin_function)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func gets(var buf)
            s := buf.size
            var pos := 0u
            for pos < s
                c := __builtin_getchar()
                if c == '\n'
                    ret buf
                end

                buf[pos] = c
                pos += 1u
            end
            ret buf
        end

        func main
            s := gets(new [char]{256u, '\0'})
            for c in s
                if c == '\0'
                    ret 0
                end
                print(c)
            end

            ret 0
        end
    )");
}

BOOST_AUTO_TEST_CASE(range)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        import std.range

        func main
            (1..10).each do |i|
                i.println
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            (1..10).each do |i|
                i.println
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(fatal)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            fatal()
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            fatal("because you use such a Dachs!")
        end
    )");
}

BOOST_AUTO_TEST_CASE(binary_operator)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            v

            func <(r : X)
            end

            func ==(r : X)
            end
        end

        func +(l : X, r : X)
        end

        func /(l, r)
        end

        func main
            l := new X{42}
            r := new X{10}

            l < r
            l == r
            l + r
            l / 40
            3.14 / r
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            v

            func [](idx)
                ret @v[idx as uint]
            end
        end

        func [](x : X, c : char)
            ret x.v[(c - 'a') as int]
        end

        func main
            x := new X{[1, 3, 5, 7]}
            x[2].println
            x['c'].println
        end
    )");
}

BOOST_AUTO_TEST_CASE(unary_operator)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class MyInt
          - value : int

            func +(rhs : MyInt)
                ret new MyInt{@value + rhs.value}
            end

            func -
                ret new MyInt{-@value}
            end

            func !
                ret @value == 0
            end

            func <<(rhs : MyInt)
                ret new MyInt{@value << rhs.value}
            end

            func to_int
                ret @value
            end
        end

        func -(lhs : MyInt, rhs : MyInt)
            ret new MyInt{lhs.to_int - rhs.to_int}
        end

        func +(operand : MyInt)
            ret operand
        end

        func >>(lhs : MyInt, rhs : MyInt)
            ret new MyInt{lhs.to_int >> rhs.to_int}
        end

        func main
            i := new MyInt{42}
            var j := new MyInt{10}
            i + j
            i - j
            i << j
            i >> j
            -i
            (+j).to_int.println
            (!!i).println

            var a := [i, j]
            k := a[0] + a[1]
            a[0] = k + a[1]
        end
    )");
}

BOOST_AUTO_TEST_CASE(compound_assignments)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
    class X
        a

        func +(r : X)
            ret new X{@a + r.a}
        end
    end

    func f
        ret (11, '0', new X{11})
    end

    func main
        var x := new X{42}
        var y, var z := x, x

        x += y
        z = x
        x, y += y, z

        var a := [x, y, z]
        a[0], a[1] += a[1], a[2]

        var t := (1, 'a', x)

        var i := 42
        var c := 'a'

        i, c += 10, 'c'

        i, c, z += t

        x, i, z += z, 10, x

        y += begin
            var i := 42
            var c := 'b'
            var z := x

            i, c, z += t
            i, c, z += f()

            t[2]
        end
    end
    )");
}

BOOST_AUTO_TEST_CASE(static_array)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func show_1(a : static_array)
            a[1].println
        end

        func show_0(a)
            a[0].println
        end

        func main
            v := new static_array(int) {3u, 3}
            for e in v
                e.println
            end
            v.size.println
            show_1(v)

            var c := new static_array {3u, 'a'}
            c[0] = 'b'
            c[1] = 'c'
            for e in c
                e.println
            end
            c.size.println
            c.show_0

            d := new static_array(int) {3u}
            for e in d
                e.println
            end
            d.size.println
            d.show_1

            new static_array {2u, 'a'} : static_array(char)
            new static_array {2u, 'a'} : static_array
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := new static_array{2u, 1}
            b : static_array(int) := a
            (b : static_array(int))[1].println
            (b : static_array)[1].println
            b2 : static_array := a
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(a : static_array)
            for e in a
                e.println
            end
        end

        func main
            a := new static_array(int){3u}
            foo(a)

            for e in new static_array(int){3u}
                e.println
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(pointer)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            begin
                var i := 42u
                var p := new pointer(int){i}
                p[3] = -30
                p[3].println
            end

            begin
                var p := new pointer(int){42u}
                p[3] = -30
                p[3].println
            end

            begin
                var i := 0u
                new pointer(int){i}
                new pointer(int){0u}
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var s := new pointer(char){4u}
            s[0] = 'i'
            s[1] = 'n'
            s[2] = 'u'
            s[3] = '\0'
            s.println
            s[2].println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(p)
        end

        func foo2(p : pointer(char))
        end

        func foo3(p : pointer)
        end

        func foo4(var p)
        end

        func foo5(var p : pointer(char))
        end

        func foo6(var p : pointer)
        end

        func main
            var s := new pointer(char){4u}
            s = new pointer(char){10u}

            var i := 3u
            s = new pointer(char){i}

            i = 0u
            s = new pointer(char){0u}
            s = new pointer(char){i}

            s : pointer(char)
            s : pointer

            foo(s)
            foo2(s)
            foo3(s)
            foo4(s)
            foo5(s)
            foo6(s)
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var p := new pointer(pointer(char)){4u}
            var i := 0u
            for i < 4u
                p[i] = new pointer(char){4u}
                p[i][0] = 'i'
                p[i][1] = 'n'
                p[i][2] = 'u'
                p[i][3] = '\0'
                i += 1u
            end

            i = 0u
            for i < 4u
                p[i].println
                i += 1u
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            p1 := new pointer(int){0u}
            p2 := new pointer(float){1u}

            p1.__builtin_null?.println
            p2.__builtin_null?.println
            __builtin_null?(p1).println
            __builtin_null?(p2).println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func main
            var p := new X{new pointer(int){3u}}
            p.a[0] = 10
            p.a[0].println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        import std.numeric

        func main
            s := 3u
            var p := new pointer(pointer(int)){s}
            (0...3).each do |i|
                p[i] = new pointer(int){3u}
            end

            (0...3).each do |i|
                (0...3).each do |j|
                    p[i][j] = i * j + i * j
                end
            end

            (0...3).each do |i|
                (0...3).each do |j|
                    p[i][j].println
                end
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        import std.numeric

        class X
            a
        end

        func main
            s := 3u
            var p := new pointer(pointer(X(int))){s}
            (0...3).each do |i|
                p[i] = new pointer(X(int)){3u}
            end

            (0...3).each do |i|
                (0...3).each do |j|
                    p[i][j] = new X{i * j + i * j}
                end
            end

            (0...3).each do |i|
                (0...3).each do |j|
                    p[i][j].a.println
                end
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        import std.numeric

        func main
            var p := new pointer(pointer(int)){3u}

            begin
                var q := p

                (0..2).each do |i|
                    q[i] = new pointer(int){4u}
                    var r := q[i]
                    begin
                        (0..3).each do |j|
                            r[j] = i * j
                        end
                        ()
                    end
                end
            end

            (0..2).each do |i|
                (0..3).each do |j|
                    p[i][j].println
                end
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(typeof)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        func foo(a)
            ret a + 42
        end

        func main
            var i := 42
            x := new X(typeof(foo(i)))
            i : typeof(x.a * 2)
        end
    )");
}

BOOST_AUTO_TEST_CASE(cast)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a

            cast : char
                ret 'a'
            end

            cast : Bar
                ret new Bar
            end
        end

        class Bar
            cast : int
                ret 42
            end
        end

        cast(b : Bar) : Foo(float)
            ret new Foo{3.14}
        end

        cast(f : Foo(char)) : char
            ret f.a
        end

        cast(i : int) : Foo(int)
            ret new Foo{42}
        end

        func main
            f := new Foo{42}
            (f as char).println
            f2 := new Foo{'p'}
            (f2 as char).println

            (42 as Foo(int)).a.println
            (f2 as Foo(char)).a.println
            f as Bar
            f2 as Bar

            var b := new Bar
            (b as int).println
            (b as Foo(float)).a.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(func_type)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func foo(i : int)
            println(i)
        end

        func foo2(i)
            println(i * i)
        end

        func apply(f, i)
        end

        func main
            var f := foo as func(int)
            f = foo2 as func(int) : ()
            f = (-> x in println(x)) as func(int)
            f(42)
            apply(f, 42)

            var arr := new static_array(func(int) : ()){3u, f}
            arr[0] = foo as func(int)
            arr[1] = foo2 as func(int)
            arr[2] = arr[0]
            for fn in arr
                fn(11)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class fizzbuzz
            f, b

            func exec(i)
                case
                when i % 15 == 0
                    @f(i).print; @b(i).println
                when i %  3 == 0
                    @f(i).println
                when i %  5 == 0
                    @b(i).println
                else
                    println(i)
                end
            end
        end

        func main
            fb := new fizzbuzz{(-> i in 'f') as func(int), (-> i in 'b') as func(int)}
            var i := 0
            for i < 20
                i += 1
                fb.exec(i)
            end
        end
    )");

    // Captured lambda can't be casted to function type
    CHECK_THROW_CODEGEN_ERROR(R"(
        func main
            i := 42
            (-> x in i + x) as func(int)
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
