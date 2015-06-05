#define BOOST_TEST_MODULE AnalyzerTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "test_helper.hpp"

#include <string>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/exception.hpp"

#include <boost/test/included/unit_test.hpp>

static dachs::syntax::parser p;

#define CHECK_THROW_SEMANTIC_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            dachs::syntax::importer i{{}, "test_file"}; \
            BOOST_CHECK_THROW(dachs::semantics::analyze_semantics(t, i), dachs::semantic_check_error); \
        } while (false);

#define CHECK_NO_THROW_SEMANTIC_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            dachs::syntax::importer i{{}, "test_file"}; \
            BOOST_CHECK_NO_THROW(dachs::semantics::analyze_semantics(t, i)); \
        } while (false);

#define CHECK_THROW_NOT_IMPLEMENTED_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            dachs::syntax::importer i{{}, "test_file"}; \
            BOOST_CHECK_THROW(dachs::semantics::analyze_semantics(t, i), dachs::not_implemented_error); \
        } while (false);

BOOST_AUTO_TEST_SUITE(analyzer)

BOOST_AUTO_TEST_CASE(symbol_duplication_ok)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            # functions
            func foo
            end

            func foo'
            end

            func foo? : bool
                ret true
            end

            proc bar
            end

            func main
                # local variables
                a, b := 1, 2
                var p : int, var q : int
            end

            # underscores
            func baz
                a, _ := 42, 53
                b, _ := 'a', 'b'
            end

            func baz2(_, _)
            end

            func baz3(_)
                _ := 42
            end

            # Below is OK because the variable just shadows the parameter.
            func qux(a)
                a := 1
            end

            func qux2(a)
                var a : int
            end
        )");
}

BOOST_AUTO_TEST_CASE(function_duplication_error)
{

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # function duplication
            func aaa
            end

            func aaa
            end

            func main
            end
        )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # function duplication
            func aaa(a)
            end

            func main
            end

            proc aaa(a)
            end
        )");
}

BOOST_AUTO_TEST_CASE(local_variable_duplication_error)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func main
                a, a := 1, 2
            end
        )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func main
                a := 1
                a := 2
            end
        )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func main
                a := 1
                var a : int
            end
        )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func foo(a, a)
            end

            func main
            end
        )");
}

BOOST_AUTO_TEST_CASE(ufcs)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo2(i)
        end

        func main
            42.foo
        end
    )");
}

BOOST_AUTO_TEST_CASE(new_expr)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            new [symbol]{10u}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            new [symbol]{10u, "aaa"}
        end
    )");
}

BOOST_AUTO_TEST_CASE(let)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            let a := 42 in println(a)
            println(a)
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            let
                a := 42
            in if true then 12 else 13 end
        end
    )");
}

BOOST_AUTO_TEST_CASE(do_)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            do
                a := 42
                println(a)
            end
            println(a)
        end
    )");
}

BOOST_AUTO_TEST_CASE(lambda_return_type)
{
    // Deduction
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(a, p)
            if a < 0
                ret p(a)
            else
                ret foo(a-1, p)
            end
        end

        func main
            42.foo do |i|
                ret i * 2
            end.println
        end
    )");

    // Return type
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            f := -> (a, b) : int in a + b
            f(1, 2).println

            g := -> (h) : int in h(1, 2)
            g(f).println
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            f := -> (a, b) : float in a + b
            f(1, 2).println
        end
    )");

    // Compiler can compile below only when return type is specified (#68)
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            g := -> (h) : int in h(h)
            g(g)
        end
    )");
}

BOOST_AUTO_TEST_CASE(no_lambda_capture_found)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(p)
        end

        func main
            foo() do
                println(x)
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            f := -> a in x
            f(42)
        end
    )");
}

BOOST_AUTO_TEST_CASE(invalid_type)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(p : oops)
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            x : oops := 42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            x := 42 : oops
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            -> x : oops in x + x
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            a := [1, 2, 3]
            for i : oops in a
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func f(a)
            println("foo")
        end

        func f(a : double)
            println("foo for double")
        end

        func main
            g := f
            g(3.14)
        end
    )");
}

BOOST_AUTO_TEST_CASE(no_main)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo
        end
    )");
}

BOOST_AUTO_TEST_CASE(uninitialized_variable_use)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            i
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            i := i
        end
    )");
}

BOOST_AUTO_TEST_CASE(invocation_with_wrong_arguments)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i : int)
        end

        func main
            foo(1.0)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i : int)
        end

        func main
            foo(1, 2)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
        end

        func main
            foo(1, 2)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(p)
            p(1, 2)
        end

        func main
            foo() do |i|
                println(i)
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            f := -> x in x + 1
            f(1, 2)
        end
    )");
}

BOOST_AUTO_TEST_CASE(edge_case_in_UFCS_function_invocation)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(r, a, b)
        end

        func main
            a := [1, 2, 3]
            a.size + 42u    # + is treated as binary operator
            b := 42
            b.foo +42 do
            end
        end
    )");
}

BOOST_AUTO_TEST_SUITE(class_definition)
    BOOST_AUTO_TEST_CASE(instance_variable_initialization_outside_ctor)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            func main
                @foo := 42
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            func foo(@hoge)
            end

            func main
                foo(42)
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(instance_var_init_outside_class)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            func foo(@aaa)
                @aaa.println
            end

            func main
                foo(42)
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(non_exist_instance_var_init)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                init(@unknown)
                end
            end

            func main
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(instance_var_init_type_mismatch)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo : int
                init(@bar : char)
                end
            end

            func main
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(member_func_duplication)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                func foo(a : int)
                end

                func foo(a : int)
                end
            end

            func main
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                func foo(a : int, b)
                end

                func foo(a : int, b)
                end
            end

            func main
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class Foo
                func foo(a : int, b : char)
                end

                func foo(a : int, b)
                end
            end

            class Foo2
                func foo(a : int, b)
                end

                func foo(a, b)
                end
            end

            func main
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(class_duplication)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
            end

            class Foo
            end

            func main
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo : int
            end

            class Foo
                bar
            end

            func main
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(clazz)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            func foo(@aaa)
                @aaa.println
            end

            func main
                foo(42)
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                @foo : int
            end

            func main
                a := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            func main
                for @i in [1, 2, 3]
                end
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(no_matching_ctor)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int

                init(@a)
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a

                init(@a)
                end
            end

            func main
                f := new Foo{1, 2}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Y
                a

                init(@a : X)
                end
            end

            func main
                new Y{42}
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Y
                a
            end

            class Z
                a : X

                init(@a : X)
                end
            end

            func main
                new Z{new X{42}}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Y
                a
            end

            class Z
                a : X

                init(@a : Y)
                end
            end

            func main
                new Z{new Y{42}}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Z
                a : X(X(int))

                init(@a : X(X(float)))
                end
            end

            func main
                new Z{new X{new X{3.14}}}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Y
                a
            end

            class Z
                a : X(X(int))

                init(@a : X(Y(int)))
                end
            end

            func main
                new Z{new X{new Y{3}}}
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class X
                a
            end

            class Y
                a : X(int)

                init(@a : X)
                end
            end

            class Z
                a : X(X(int))

                init(@a : X(X))
                end
            end

            func main
                new Y{new X{3}}
                new Z{new X{new X{3}}}
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(undefined_class)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            func main
                new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            func foo(a : Foo)
            end

            func main
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(undefined_instance_variable)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a

                init(@a, @b)
                end
            end

            func main
                f := new Foo{1, 2}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                init
                end

                func foo
                    @a
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                init
                end
            end

            func main
                f := new Foo
                f.a
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                init
                end
            end

            func main
                f := new Foo
                f.a()
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                init
                    @aa := 42
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a

                init(@a)
                    @aa := 42
                end
            end

            func main
                f := new Foo{42}
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(invalid_ctor)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo

                init
                    @foo := 42
                    @foo := 31
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int

                init(@a : char)
                end
            end

            func main
                f := new Foo{'a'}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int

                init
                    @a := 'a'
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int

                init(@a : char)
                end
            end

            func main
                f := new Foo{'a'}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            # Class template
            class Foo
                a : int
                b

                init
                    @a := 'a'
                    @b := 42
                end
            end

            func main
                f := new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a
                init(@a)
                    @a := 42
                end
            end

            func main
                f := new Foo{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int
                init(@a)
                    @a := 42
                end
            end

            func main
                f := new Foo{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a

                init
                end
            end

            func main
                new Foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                a : int
                b

                init(a, @b)
                end

                init(a, @b)
                end
            end

            func main
                new Foo{42, 3.14}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init(@a)
                    a := @foo()
                    @b := 42
                end

                func foo : int
                end
            end

            func main
                new X
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init(@a)
                    println(@b)
                    @b := 42
                end
            end

            func main
                new X{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init(@a)
                    println(@b)
                    @b := 42
                end
            end

            func main
                new X{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init(@a)
                    self
                    @b := 42
                end
            end

            func main
                new X{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a : int
                b : symbol

                init(@a)
                    self
                end
            end

            func main
                new X{42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init(@a)
                    self
                end
            end

            func main
                new X(int, symbol){42}
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(function_duplication_check_for_class_template_param)
    {
        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    func foo(i)
                    end

                    func foo2(i : int)
                    end

                    func foo3(i)
                    end

                    func foo3(i : int)
                    end

                    func foo3(i : Foo)
                    end
                end

                func foo(f, i)
                end

                func foo2(f, i)
                end

                func foo3(f, i)
                end

                func foo3(f, i : int)
                end

                func foo3(f, i : Foo)
                end

                func main
                end
            )");
    }

    BOOST_AUTO_TEST_CASE(class_member_accessibility)
    {
        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
                # class template
                class Foo
                    a
                  - b

                    init(@a, @b)
                    end

                    func foo
                        @a.println
                        @b.println
                        self.a.println
                        self.b.println
                    end
                end

                func main
                    f := new Foo{42, 3.14}
                    f.a.println
                    f.foo()
                end
            )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
                # class template
                class Foo
                    a : int
                  - b : float

                    init(@a, @b)
                    end

                    func foo
                        @a.println
                        @b.println
                        self.a.println
                        self.b.println
                    end
                end

                func main
                    f := new Foo{42, 3.14}
                    f.a.println
                    f.foo()
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                  - a

                    init(@a)
                    end
                end

                func main
                    f := new Foo{42}
                    f.a
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                  - a : int

                    init(@a)
                    end
                end

                func main
                    f := new Foo{42}
                    f.a
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                  - a : int

                    init(@a)
                    end
                end

                class Other
                    init
                    end

                    func foo(f)
                        f.a
                    end
                end

                func main
                    f := new Foo{42}
                    a := new Other
                    a.foo(f)
                end
            )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    init
                    end

                    func foo
                        @bar()
                        println("foo")
                    end

                - func bar
                        println("bar")
                    end
                end

                class FooTemplate
                    a

                    init(@a)
                    end

                    func foo
                        @bar()
                        println("foo")
                    end

                - func bar
                        println("bar")
                    end
                end

                func main
                    f := new Foo
                    f.foo   # ufcs_invocation
                    f.foo() # func_invocation

                    f2 := new FooTemplate{42}
                    f2.foo   # ufcs_invocation
                    f2.foo() # func_invocation
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    init
                    end

                  - func foo
                    end
                end

                func main
                    f := new Foo
                    f.foo
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    a

                    init(@a)
                    end

                  - func foo
                    end
                end

                func main
                    f := new Foo{42}
                    f.foo
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    init
                    end

                  - func foo
                    end
                end

                func main
                    f := new Foo
                    f.foo
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    a

                    init(@a)
                    end

                  - func foo
                    end
                end

                func main
                    f := new Foo{42}
                    f.foo()
                end
            )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
                class Foo
                    a

                    init(@a)
                    end

                  - func foo
                    end
                end

                class Other
                    init
                    end

                    func foo(f)
                        f.foo
                    end
                end

                func main
                    o := new Other
                    o.foo(new Foo{42})
                end
            )");

        // Member function will be called instead of private member access.
        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class Foo
              - foo : int

                init
                end

                func foo
                    "foo".println
                end
            end

            func main
                f := new Foo
                f.foo
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo : int

                init
                end

              - func foo
                    "foo".println
                end
            end

            func main
                f := new Foo
                f.foo.println
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
              - foo : int

                init
                end

              - func foo
                    "foo".println
                end
            end

            func main
                f := new Foo
                f.foo.println
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(non_exist_member)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Template
                a

                init(@a)
                end
            end

            func main
                t := new Template{42}
                t.foo
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class NonTemplate
                init
                end
            end

            func main
                t := new NonTemplate
                t.foo
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(constuct_with_template_types)
    {
        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class Template
                a

                init(@a)
                end
            end

            class Template2
                a
                b : float

                init(@a, @b)
                end
            end

            class NonTemplate
                a : int

                init
                    @a := 42
                end
            end

            class X2
                a, b

                init(@a, @b)
                end
            end

            class X
                a

                init
                end
            end

            func main
                new Template(int){42}
                new Template(float){3.14}

                new NonTemplate

                new Template2(int){42, 3.14}
                new Template2(char){'a', 3.14}

                new Template(Template(NonTemplate)){new Template{new NonTemplate}}

                new X(int)
                new X(X(int))
                new X2(X(int), X2(int, X(int))){new X(int), new X2{42, new X(int)}}

                new X(int) # X can't be instantiated without specifying type
                new X(float) # X can't be instantiated without specifying type
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a : int

                init(@a)
                end
            end

            func main
                x := new X(int){42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(@a)
                end
            end

            func main
                x := new X(int, int){42}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(@a)
                end
            end

            func main
                x := new X(int){3.14}
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a, b

                init
                end
            end

            func main
                new X(int, X(int, X)){42, new X(int, X){new X}}
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init
                end
            end

            func main
                x := new X(X(int))
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(@a : int)
                end
            end

            func main
                x := new X(float){42}
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(instantiated_class_template_as_parameter_type)
    {
        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init
                end
            end

            func foo(x : X(int))
            end

            func foo(x : X(float))
            end

            func foo'(x : X)
            end

            func foo''(x : X(int), y : int)
            end

            func foo''(x : X(int), y : float)
            end

            func main
                x := new X(int)
                x2 := new X(float)
                foo(x)
                foo(x2)
                foo'(x)
                foo'(x2)
                foo''(x, 1)
                foo''(x, 1.0)
            end
        )");

        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init
                end
            end

            func foo(x : X(int))
            end

            func foo(x : X(int))
            end

            func main
            end
        )");

        // TODO:
        // More test cases should be added.
    }

    BOOST_AUTO_TEST_CASE(implicit_default_constructor)
    {
        // If any ctor already exists, not defined implicitly.
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                init(a)
                end
            end

            func main
                new X
            end
        )");

        // If any ctor already exists, not defined implicitly.
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(@a)
                end
            end

            func main
                new X
            end
        )");

        // If any instance var is not default constructible, not defined implicitly
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a : symbol
                b : int
            end

            func main
                new X
            end
        )");

        // If any instance var is not default constructible, not defined implicitly
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a : symbol
                b
            end

            func main
                new X
            end
        )");

        // Check nested template type
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(@a)
                end
            end

            class Y
                a, b
            end

            func main
                new Y(int, Y(X(int), X(symbol)))
            end
        )");

        // Check nested template type
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a : symbol
                init
                    @a := ""
                end
            end

            class Y
                a, b
            end

            func main
                new Y(int, Y(X, X))
            end
        )");

        // Ctor arguments have higher priority than do-end block
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                init(block)
                    block().println
                end
            end

            func main
                new X { 42 }
            end
        )");
    }

    BOOST_AUTO_TEST_CASE(implicit_memberwise_constructor)
    {
        // If any ctor already exists, not defined implicitly.
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class X
                a

                init(a, b)
                    @a := a + b
                end
            end

            func main
                new X{42}
            end
        )");
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(non_paren_func_calls_edge_cases)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a

            init(@a)
            end

            func b(x)
                @a[0]
            end
        end

        func c(x, b)
        end

        func main
            f := new Foo{[42]}
            f.a[0]  # Index access
            f.b [0] # UFCS invocation

            c [0] do # Function invocation with do-end block
                println("foo")
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(builtin_names_check)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class __builtin_foo
        end
        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func __builtin_foo
        end
        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            __builtin_foo := 42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(__builtin_arg)
        end
        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class foo
            __builtin_foo
        end
        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for __builtin_foo in [1, 2, 3]
            end
        end
    )");

    // Intrinsics
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            __builtin_read_cycle_counter.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(const_instance_var_access_check)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a

            init(@a)
            end
        end

        func main
            f := new Foo{42}
            f.a = 42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a : int

            init(@a)
            end
        end

        func main
            f := new Foo{42}
            f.a = 42
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a : int

            init(@a)
            end
        end

        class Foo2
            a : int

            init(@a)
            end
        end

        func main
            var f := new Foo{42}
            f.a = 42
            var f2 := new Foo2{42}
            f.a = 42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a

            init(@a)
            end
        end

        func main
            f := new Foo{new Foo{42}}
            f.a.a = 42
        end
    )");
}

BOOST_AUTO_TEST_CASE(const_func_access_check)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Template
            a

            init(@a)
            end

            func modify
                if @a > 21
                    @a = 42
                end
            end
        end

        func main
            t := new Template{42}
            t.modify()
        end
    )");

    // UFCS
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Template
            a

            init(@a)
            end

            func modify
                if @a > 21
                    @a = 42
                end
            end
        end

        func main
            t := new Template{42}
            t.modify
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class NonTemplate
            a : int

            init
                @a := 42
            end

            func modify
                if @a > 21
                    @a = 42
                end
            end
        end

        func main
            n := new NonTemplate
            n.modify()
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class NonTemplate
            a : int

            init
                @a := 42
            end

            func non_modify
                @a.println
            end
        end

        func main
            n := new NonTemplate
            n.non_modify()
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class Template
            a

            init(@a)
            end

            func modify
                if @a > 21
                    @a = 42
                end
            end
        end

        class NonTemplate
            a : int

            init
                @a := 42
            end

            func modify
                if @a > 21
                    @a = 42
                end
            end
        end

        func main
            var t := new Template{42}
            t.modify()

            var n := new NonTemplate
            n.modify()
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a : int

            init
                @a := 42
            end

            func modify
                @a = 10
            end
        end

        func main
            f := new Foo
            (f : Foo).modify()
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a : int

            init
                @a := 42
            end

            func modify
                @a = 10
            end
        end

        func main
            f := [new Foo, new Foo]
            f[0].modify()
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
          - a
          - b

            init(@a, @b)
            end

            func set_a(a)
                @a = a
            end

            func set_b(b)
                @b = b
            end

            func set_all(a, b)
                @set_a(a)
                @set_b(b)
            end
        end

        func main
            f := new Foo{42, 3.14}
            f.set_all(10, 10.1)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a

            init(@a)
            end

            func modify
                @a.a = 10
            end
        end

        func main
            f := new Foo{new Foo{42}}
            f.modify
        end
    )");
}

BOOST_AUTO_TEST_CASE(typed_expression)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        class Y
            a, b

            init(@a, @b)
            end
        end

        class Z
            a : int
            init
            end
        end

        func main
            let
                x := new X{24}
            begin
                x : X(int)
                x : X
            end

            let
                y := new Y{new X{42}, new X{3.14}}
            begin
                y : Y(X(int), X(float))
                y : Y(X, X(float))
                y : Y(X(int), X)
                y : Y(X, X)
                y : Y
            end

            let
                x := new X{new X{new X{42}}}
            begin
                x : X(X(X(int)))
                x : X(X(X))
                x : X(X)
                x : X
            end

            let z := new Z in z : Z
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func main
            x := new X{42}
            x : float
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func main
            x := new X{42}
            x : X(float)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        class Y
            a : int

            init(@a)
            end
        end

        func main
            x := new X{42}
            x : Y
        end
    )");
}

BOOST_AUTO_TEST_CASE(overload)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            init
            end
        end

        func foo(x)
        end

        func foo(x : X)
        end

        func foo(x : int)
        end

        class Y
            a

            init(@a)
            end
        end

        func foo(x : Y)
        end

        func foo(x : Y(Y))
        end

        func foo(x : Y(Y(int)))
        end

        func foo(x : Y(Y), i : int, j : int)
        end

        func foo(x : Y(Y(int)), i : int, j)
        end

        func foo(x : Y(Y), i : float, j : int)
        end

        func foo(x : Y(Y(int)), i : float, j : int)
        end

        func foo(a1 : int, a2 : Y, a3 : Y(Y), a4 : Y(Y(int)), a5 : X, a6 : string, a7)
        end

        func foo(a1 : int, a2 : Y, a3 : Y, a4 : Y(Y(int)), a5 : X, a6 : string, a7)
        end

        func foo(a1 : int, a2 : Y, a3 : Y, a4 : Y(Y), a5 : X, a6 : string, a7)
        end

        func foo(a1, a2 : Y, a3 : Y, a4 : Y, a5 : X, a6, a7)
        end

        func foo(a1, a2, a3, a4, a5, a6, a7)
        end

        func foo(x : X, y)
        end

        func foo(x, y : X)
        end

        func foo(x : X, y : X)
        end

        func main
            foo(42)
            foo(new X)
            foo(3.14)

            foo(new Y{new X})
            foo(new Y{new Y{42}})
            foo(new Y{new Y{3.14}})

            foo(new Y{new Y{42}}, 42, 42)
            foo(new Y{new Y{42}}, 42, 3.14)

            foo(new Y{new Y{3.14}}, 3.14, 42)
            foo(new Y{new Y{42}}, 3.14, 42)

            foo(42, new Y{42}, new Y{new Y{3.14}}, new Y{new Y{42}}, new X, "aaa", 'a')
            foo(42, new Y{42}, new Y{3.14}, new Y{new Y{42}}, new X, "bbb", 'a')
            foo(42, new Y{42}, new Y{3.14}, new Y{new Y{3.14}}, new X, "ccc", 'a')
            foo('a', new Y{42}, new Y{3.14}, new Y{new Y{'a'}}, new X, [1, 2, 3], 'a')
            foo('a', "aaa", [1, 2], 42, 3.14, new Y{42}, new X)

            foo(new X, new X)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func foo(x : X(float))
        end

        func main
            foo(new X{42})
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func foo(x : X(X(float)))
        end

        func main
            foo(new X{new X{42}})
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            init
            end
        end

        func foo(x : X, y)
        end

        func foo(x, y : X)
        end

        func main
            foo(new X, new X)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func foo(x : X, y)
        end

        func foo(x, y : X)
        end

        func main
            foo(new X{42}, new X{42})
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func foo(x : X(int), y)
        end

        func foo(x, y : X(int))
        end

        func main
            foo(new X{42}, new X{42})
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            init(@a)
            end
        end

        func foo(x : X(X), y)
        end

        func foo(x, y : X(X))
        end

        func main
            x := new X{new X{42}}
            foo(x, x)
        end
    )");


    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(x, y : int)
        end

        func foo(x : int, y)
        end

        func main
            foo(42, 42)
        end
    )");
}

BOOST_AUTO_TEST_CASE(main_func)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main(args)
            args[0]
            args.size
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main(args : argv)
            args[0]
            args.size
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main(args : int)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main(var args)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main : int
            ret main()
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class Foo
            a

            func main
            end

            func main(a)
            end
        end

        func main
            (new Foo{42}).main
            (new Foo{42}).main(42)
        end
    )");
}

BOOST_AUTO_TEST_CASE(invalid_index)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            a := [1, 2]
            a['a']
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            t := (1, 'a')
            t['a']
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            t := (1, 'a')
            i := 1
            t[i]
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            t := (1, 'a')
            t[10000]
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            t := (1, 'a')
            t[-1]
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            s := "aaa"
            s['a']
        end
    )");
}

BOOST_AUTO_TEST_CASE(invalid_var_def_without_init)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var x : symbol
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a : symbol
        end

        func main
            var x : X
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        func main
            var x : X(symbol)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        func main
            var x : X(X(symbol))
        end
    )");
}

BOOST_AUTO_TEST_CASE(instance_var_invocation_vs_ufcs_invocation)
{
    // instance variable has higher priority so it wins
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            foo

            func foo(i)
                println("member func")
            end
        end

        func main
            x := new X {|i| println("instance var")}
            x.foo(42) # "instance var"
        end
    )");

    // member function is invoked becaues @foo is not callable
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            foo

            func foo(i)
                println("member func")
            end
        end

        func main
            x := new X{42}
            x.foo(42) # "member func"
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            foo
        end

        func main
            x := new X{42}
            x.foo()
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            foo

            func foo
                println("member func")
            end
        end

        func main
            x := new X {-> println("instance var")}
            x.foo() # "instance var"
        end
    )");

    // member function is invoked becaues @foo is not callable
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            foo

            func foo
                println("member func")
            end
        end

        func main
            x := new X{42}
            x.foo # "member func"
        end
    )");
}

BOOST_AUTO_TEST_CASE(operator_type_check)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            10 + 1u
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            10.0 / 5
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var i := 10u
            i += 1
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            'a' < 1
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        func main
            new X{10} < new X{11}
        end
    )");
}

BOOST_AUTO_TEST_CASE(builtin_type_operator_check)
{
    // Binary operators for built-in types can't be defined by user.
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func [](i : int, j : int)
        end

        func main
            3[4]
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func >>(c : char, j : int)
        end

        func main
            'a' >> 3
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func >>(s, j)
        end

        func main
            'b' >> 3
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func !(i : int)
            ret i == 0
        end

        func main
            !42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func !(i)
            ret (i as int) == 0
        end

        func main
            !42
        end
    )");
}

BOOST_AUTO_TEST_CASE(operator_arguments_check)
{
    // Unary only
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func !
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func !(x, y)
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func !(x)
            end
        end

        func main
        end
    )");

    // Binary only
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func >>(i, j, k)
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func >>
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func >>(x, y)
            end
        end

        func main
        end
    )");

    // Unary or binary
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func +(i, j, k)
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func +
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func +(x, y)
            end
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func []=(i)
            end
        end

        func main
        end
    )");
}

BOOST_AUTO_TEST_CASE(compound_asssignment)
{
    // Built-in
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var i := 0
            i += 1u
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            v : int

            func +(i : int)
                ret new X{@v + i}
            end
        end

        func +(i : int, x : X)
            ret i + x.v
        end

        func main
            var x := new X{10}
            x += 11

            var i := 3
            i += x
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            v : int

            func +(i : int)
            end
        end

        func main
            var x := new X{10}
            x += 3.14
        end
    )");
}

BOOST_AUTO_TEST_CASE(switch_stmt)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            case 42
            when bool
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func main
            x := new X
            case x
            when 42
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func main
            x := new X
            case x
            when 42
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(assignment_stmt)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var i := 42
            var c := 'a'
            i, c = c, i
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var i := 42
            var c := 'a'
            var k := 3.14
            i, c, k = c, i
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var i := 42
            var c := 'a'
            i, c = i
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var t := (42, 'a')
            t = 21, 'b'
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            var t := (42, 'a')
            t = (21, 'b')
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var p := new pointer(int){1u}
            p[3.14] = 42
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            var p := new pointer(int){1u}
            p[0] = 3.14
        end
    )");
}

BOOST_AUTO_TEST_CASE(for_stmt)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func size(_ : X)
            ret 0u
        end

        func main
            for e in new X
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func [](_ : X, i : uint)
            ret 'a'
        end

        func main
            for e in new X
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func [](_ : X, i : int)
            ret 'a'
        end

        func size(_ : X)
            ret 3u
        end

        func main
            for e in new X
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func [](_ : X, i : uint)
            ret 'a'
        end

        func size(_ : X)
            ret 3
        end

        func main
            for e in new X
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(type_specifier)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X1
            func foo(a : Y)
            end
        end

        class X2
            a : Y
        end

        class X3
            func foo : Y
                ret new Y
            end
        end

        class Y
        end

        func main
            var i : int
            i : int
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X1
            func foo(a : Y)
            end
        end

        class X2
            a : Y
        end

        class X3
            func foo : Y
                ret new Y{42}
            end
        end

        class Y
            a
        end

        func main
            var y : Y := new Y{42}
            y : Y
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func foo : Y(float)
                ret new Y{42}
            end
        end

        class Y
            a
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func foo : float
                ret 42
            end
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            func foo : Y
                ret 42
            end
        end

        class Y
            a
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X1
            func foo(a : Y(foo))
            end
        end

        class Y
            a
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X2
            a : Y(foo)
        end

        class Y
            a
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X3
            func foo : Y(foo)
                ret new Y(foo)
            end
        end

        class Y
            a
        end

        func main
        end
    )");
}

BOOST_AUTO_TEST_CASE(pointer)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            new pointer(int){1u, 'a'}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            new pointer(int){1}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        func main
            new X{new pointer{2u}}
        end
    )");
}

BOOST_AUTO_TEST_CASE(typeof)
{
    CHECK_THROW_NOT_IMPLEMENTED_ERROR(R"(
        func foo(a, b : typeof(a))
        end

        func main
        end
    )");

    CHECK_THROW_NOT_IMPLEMENTED_ERROR(R"(
        class X
            a, b : typeof(a)
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            42 : typeof(unknown)
        end
    )");
}

BOOST_AUTO_TEST_CASE(copier)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            copy
                ret new X
            end
        end

        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            copy
                ret 42
            end
        end

        func main
        end
    )");

    // Note:
    // Below test cases have an invalid deep copy operator definition
    // (it should return X(int) but actually do int).
    // So, when the deep copy operator is checked, it occurs error although
    // no error occurs when the deep copy is not checked.
    // So error means that 'deep copy is correctly analyzed'.
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        class Y
            a

            init(a)
                @a := a
            end

            init(@a, i : int)
            end
        end

        func main
            var x := new X{42}
            new Y{x}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            var xs := [x]
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            (x, 42)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            x = new X{21}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            var xs := [x]
            xs[0] = x
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func foo(var x : X)
        end

        func main
            var x := new X{42}
            foo(x)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        class Y
            a

            init(@a)
            end
        end

        func main
            var x := new X{42}
            new Y{x}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            new static_array{3u, x}
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a

            copy
                ret 42
            end
        end

        func main
            var x := new X{42}
            var xs := [x]
            for var e in xs
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(tuple_traverse)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            for e in (1, 'a')
            end

            for a, b in ((1, 'a'), (3.14, 10u))
            end

            for var a, var b in ((1, 'a'), (3.14, 10u))
            end

            for a : int, var b in ((1, 'a'), (3, 10u))
            end

            for a, b in ((1, 'a'), (3.14, 10u))
                for var c, var d in ((1, 'a'), (3.14, 10u))
                    for e : int, var f in ((1, 'a'), (3, 10u))
                    end
                end
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for e : char in (1, 2)
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for a, b in (1, 2)
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for a, b in ((1, 2), (1, 2, 3))
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for a : int, b in ((1, 2), ('a', 'b'))
            end
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for a : int, b in ((1, 2), ('a', 'b'))
            end
        end
    )");

    // Note: Failure on forward analyzer in block
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            for a in (1, 'a')
                __builtin_foo := a
            end
        end
    )");

    // Note: Failure on analyzer in block
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        end

        func foo(a : int)
        end

        func main
            for e in (1, new X)
                foo(e)
            end
        end
    )");
}

BOOST_AUTO_TEST_CASE(conversion)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        cast(i : int) : char
            ret 'a'
        end
        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        cast(p : pointer(char)) : char
            ret p[0]
        end
        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        cast(i : uint) : pointer(int)
            ret new pointer(int){i}
        end
        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        cast(i) : X; end
        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        cast(x : X) : int; ret 42 end
        cast(x : X) : int; ret 42 end
        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            cast : int; ret 42 end
        end
        cast(x : X) : int; ret 42 end
        func main; end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X; end
        func main
            X as X
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        cast(i : int) : X(int)
            ret new X{i}
        end

        func main
            42 as X
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            a
            cast : int
                ret 42
            end
            cast : char
                ret 'a'
            end
        end

        cast(x : X(int)) : int
            ret x.a
        end

        cast(x : X(int)) : char
            ret x.a as char
        end

        func main; end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        func main
            42 as X
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        func main
            new X as int
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        cast(i : int) : X; end
        func main
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X; end
        cast(i : int) : X(int)
            ret new X
        end
        func main
        end
    )");
}

BOOST_AUTO_TEST_CASE(type_inspection)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func main
            (1, 'a', 3.14).__type.println
        end
    )");
}

BOOST_AUTO_TEST_CASE(array)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
            a
        end

        func main
            [] : [X]
        end
    )");
}

BOOST_AUTO_TEST_CASE(function_conversion)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(a, b)
            ret a + b
        end

        func bar(a, b)
            ret true
        end

        class X
            a
        end

        func baz(a, b : X)
            ret a + b.a
        end

        func main
            foo as func(int, int)
            foo as func(int, int) : int
            foo as func(char, char)
            foo as func(char, char) : char
            bar as func(char, char)
            bar as func(char, char) : bool
            baz as func(int, X(int)) : int

            (foo as func(int, int))(1, 2)
            x := new X{42}
            b := baz as func(int, X(int))
            b(x.a, x)
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
        end

        func bar(i : int)
        end

        func main
            var f := bar as func(int)
            b := f as func(int)
            f = b
            f2 := bar as func(int)
            (f2 as func(int))(42)
        end
    )");

    // Overload
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
            ret i
        end

        class X
        end

        func main
            f := foo as func(int)
            g := foo as func(char)
            h := foo as func(X)

            f(42); g('c'); h(new X)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j)
            ret i + j
        end

        func main
            foo as func(int)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j : int)
            ret i + j
        end

        func main
            foo as func(int, char)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j : int)
            ret i + j
        end

        func main
            foo as func(int, char)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j)
            ret i + j
        end

        func main
            foo as func(int, char)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j)
            ret i + j
        end

        func main
            f := foo as func(int, int)
            f(1, 2, 3)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j)
            ret i + j
        end

        func main
            f := foo as func(int, int)
            f(1, 2.0)
        end
    )");

    // Note:
    // Funciton type to function type conversion (check)

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
            ret i as float
        end

        func main
            f := foo as func(int)
            f as func(int): int
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
            ret i as float
        end

        func main
            f := foo as func(int)
            f as func(char): float
        end
    )");

    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        func foo(i)
            ret i as float
        end

        func main
            f := foo as func(int) : float
            f as func(int)
            var g := foo as func(int)
            g = g as func(int)
            g = g as func(int): float
            g = f
        end
    )");

    // Private functions
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
        - func foo(c : char)
            end

            func foo(i : int)
            end
        end

        func foo(c : char)
        end

        func main
            foo as func(X, int)
            foo as func(char)
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        class X
        - func foo(c : char)
            end

            func foo(i : int)
            end
        end

        func foo(c : char)
        end

        func main
            foo as func(X, char)
        end
    )");

    // Edge case: functional member variable
    // On member function call @foo(), compiler looks up
    // the member of class at first.
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
        class X
            f, g, h

            func i
            end

            func foo
                @f(); @g(); @h(); @i()
            end
        end

        func blah
        end

        func main
            x := new X{blah as func(), blah, -> 42}
            x.foo
        end
    )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            main as func()
        end
    )");

    // Ambiguous instantiation
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func foo(i, j : int)
        end
        func foo(i : int, j)
        end

        func main
            foo as func(int, int)
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
