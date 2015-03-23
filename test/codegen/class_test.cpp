#define BOOST_TEST_MODULE LLVMCodegenClassTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"
#include "./codegen_test_helper.hpp"

using namespace dachs::test;

BOOST_AUTO_TEST_SUITE(codegen_llvm)
BOOST_AUTO_TEST_SUITE(class_definition)

BOOST_AUTO_TEST_CASE(empty)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            init
            end
        end

        class Y
            init
            end
        end

        func main
            x := new X
        end
    )");
}

BOOST_AUTO_TEST_CASE(init_in_param_of_ctor)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a : int
            init(@a)
            end
        end

        class Foo2
            a
            init(@a)
            end
        end

        func main
            a := new Foo{42}
            b := new Foo2{42}
            a.a.println
            b.a.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(init_in_body_of_ctor)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a : int
            b

            init(a)
                @a := a
                @b := 3.1
            end
        end

        class Foo2
            a : int
            b

            init(a : int)
                @a := a
                @b := 3.1
            end
        end

        func main
            var a := new Foo{42}
            a.a.println
            a.b.println
            b := new Foo{42}
            b.a.println
            b.b.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a : int
            b

            init(a)
                @a := a
                @b := 3.1
            end

            init
                @a, @b := 42, 3.14
            end
        end

        func main
            a1 := new Foo{42}
            a2 := new Foo
        end
    )");
}

BOOST_AUTO_TEST_CASE(general_method)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a : int

            init(@a)
            end

            func non_template(a : int)
                println(a + @a)
            end

            func template(a)
                println(a as int + @a)
            end
        end

        class FooTemplate
            a

            init(@a)
            end

            func non_template(a : int)
                println(a + @a)
            end

            func template(a)
                println(a as int + @a)
            end
        end

        func main
            do
                a := new Foo{42}
                a.non_template(42)
                a.template(3.14)
                a.template('a')
            end

            do
                a := new FooTemplate{42}
                a.non_template(42)
                a.template(3.14)
                a.template('a')
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a

            init
                @a := 42
            end

            func m1(a)
                ret a as int * @a
            end

            func m2(a)
                println(@m1(a))
            end
        end

        func main
            f := new Foo
            f.m2(42)
            f.m2(f.m1(f.a))
        end
    )");
}

BOOST_AUTO_TEST_CASE(extension_method)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a

            init
                @a := 42
            end

            func use_foo
                self.foo
                self.foo2
            end
        end

        func foo(r : Foo)
            println(r.a)
        end

        func foo2(r : Foo)
            println(r.a)
        end

        func main
            f := new Foo
            f.foo
            f.foo2
            f.use_foo
        end
    )");

    // Non-template
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a : int

            init
                @a := 42
            end

            func use_foo
                self.foo
                self.foo2
            end
        end

        func foo(r : Foo)
            println(r.a)
        end

        func foo2(var r : Foo)
            println(r.a)
        end

        func main
            f := new Foo
            f.foo
            f.foo2
            f.use_foo
        end
    )");
}

BOOST_AUTO_TEST_CASE(class_in_class)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Template
            a

            init(@a)
            end

            func foo
                ret @a
            end
        end

        class NonTemplate
            a : int

            init(@a)
            end

            func foo
                ret @a
            end
        end

        func main
            tt := new Template{new Template{42}}
            tt2 := new Template{new NonTemplate{42}}
            ttt := new Template{new Template{new Template{42}}}

            tt.foo.foo.println
            tt2.foo.foo.println
            ttt.foo.foo.foo.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(implicitly_defined_ctor)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a, b
        end

        class Y
            a : int
            b : float
        end

        class Z
            a, b
        end

        class W
        end

        func main
            x := new X(int, float)
            x.a.println
            x.b.println

            y := new Y
            y.a.println
            y.b.println

            z := new Z(X(int, float), Y)

            w := new W
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
            b
        end

        class Y
            a : char
            b : string
        end

        class Z
            p, q
        end

        func main
            x := new X{42, 3.14}
            x.a.println
            x.b.println

            y := new Y{'a', "aaa"}
            y.a.println
            y.b.println

            z := new Z{x, y}
            z.p.a.println
            z.p.b.println
            z.q.a.println
            z.q.b.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(constructor_restriction)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a : int
            b
            c

            init(@c)
                var a := 2 + @c
                a *= 4
                @a := a
                println(@a)
                @b := @a + 2

                self.b.println
                # @foo()
                self
            end

            func foo : int
                println("foo")
            end
        end

        func main
            x := new X{42}
            x.a.println
            x.b.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a

            init
                @a := 42
                @foo()
                @bar(@a)
            end

            func foo
            end

            func bar(a)
            end
        end

        func main
            new X
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a : int

            init
                @a := 42
                @foo()
                @bar(@a)
            end

            func foo
            end

            func bar(a)
            end
        end

        func main
            new X
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a

            init(@a)
                @foo()
                @bar(@a)
            end

            func foo
            end

            func bar(a)
            end
        end

        func main
            new X{42}
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a, b

            func foo
            end

            init
                @a := 42
                @b := @a
                @foo()
            end
        end

        func main
            new X
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a : int

            init
                @foo()
            end

            func foo
                @a.println
            end
        end

        class X2
            a

            init(@a)
                @foo()
            end

            func foo
                @a.println
            end
        end

        func main
            new X
            new X2{42}
        end
    )");

    // @b in X is default constructible
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a, b

            init
                @a := 42
                @foo(@b.a)
            end

            func foo(a)
                a.println
            end
        end

        class Y
            a : int
        end

        func main
            new X(int, Y)
        end
    )");

    // @b in X is default constructible
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a, b

            init
                @a := 42
                @foo(@b.a)
            end

            func foo(a)
                a.println
            end
        end

        class Y
            a
        end

        func main
            new X(int, Y(int))
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_not_degrade)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a, b

            init(@b)
                @a := @b + @b
            end
        end

        func main
            do
                f := new Foo{42}
                f.a.println
                f.b.println
            end

            do
                f := new Foo{3.14}
                f.a.println
                f.b.println
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            b

            init(@b : int)
            end
        end

        func main
            f := new Foo{42}
            f.b.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a

            init(@a)
            end

            init
                @a := new Foo{42}
            end
        end

        func main
            f := new Foo
            f.a.a.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class Foo
            a

            init(@a)
            end
        end

        func main
            var f := new Foo{new Foo{42}}
            f.a = new Foo{42}
            f.a.a.println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            init
                @foo()
            end

            func foo
                println("foo")
            end
        end

        func main
            (new X).foo
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        class X
            a
        end

        class Z
            a : X(int)

            init(@a : X)
            end
        end

        func main
            new Z{new X{3}}
        end
    )");

}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

