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
            do
                var a := [1]
                var t := ('a', a)
                print_1_0(t)
            end

            do
                var a := [1]
                t := ('a', a)
                print_1_0(t)
            end

            do
                var t := (-1, -2)
                var a := [t, t]
                print_1_0(a)
            end

            do
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
            a := new [char]{4u}
            var b := new [float]{4u}
            c := new [float]{4u, 3.14}
            var d := new [char]{4u, 'd'}
            var s := new [string]{4u, "aaa"}

            var e := new [uint]{32u}
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

            println("foo_hah" as symbol)
            s := "foo_hah"
            println(s as symbol == :foo_hah)
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
            in if a == 4
                println(a)
            else
                println(b)

            let
                a, var b := 42, 'a'
            in if a == 4 then a else (b as int)

            let
                var a := 42
                var b := 42
            in begin
                for a < 50
                    println(a)
                    a += 1
                end
            end

            foo().println

            let
                a := 42
            in begin
                case a
                when 42
                    println("42")
                end
            end

            let var a := 42 in begin a = 21 end

            let var a := 42 in begin b := a end

            let
                var a := 42
                var b := 42
            begin
                for a < 50
                    println(a)
                    a += 1
                end
            end

            let
                a := 42
            begin
                case a
                when 42
                    println("42")
                end
            end

            let
                a := 42
            begin
                case a
                when 42
                    ret "aaa"
                when -42
                    ret "bbb"
                end
            end
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
            s := gets(new [char]{256u})
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

            ret t[2]
        end
    end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
