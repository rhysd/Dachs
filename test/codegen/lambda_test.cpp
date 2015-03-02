#define BOOST_TEST_MODULE LLVMCodegenLambdaTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"
#include "./codegen_test_helper.hpp"

using namespace dachs::test;

BOOST_AUTO_TEST_SUITE(codegen_llvm)

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

BOOST_AUTO_TEST_SUITE_END()
