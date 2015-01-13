#define BOOST_TEST_MODULE AnalyzerTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "test_helper.hpp"

#include <string>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/exception.hpp"

#include <boost/test/included/unit_test.hpp>

static dachs::syntax::parser p;

#define CHECK_THROW_SEMANTIC_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            BOOST_CHECK_THROW(dachs::semantics::analyze_semantics(t), dachs::semantic_check_error); \
        } while (false);

#define CHECK_NO_THROW_SEMANTIC_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            BOOST_CHECK_NO_THROW(dachs::semantics::analyze_semantics(t)); \
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

BOOST_AUTO_TEST_CASE(let)
{
    CHECK_THROW_SEMANTIC_ERROR(R"(
        func main
            let a := 42 in pritln(a)
            println(a)
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

BOOST_AUTO_TEST_CASE(lambda_return_type_deduction)
{
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

    BOOST_AUTO_TEST_CASE(non_initialize_stmt_in_ctor)
    {
        CHECK_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo
                init
                    @foo := 42
                    @foo.println
                end
            end

            func main
            end
        )");

        CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            class Foo
                foo, bar

                init
                    @foo := 42
                    @bar := 3.14
                end
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
                init
                    42 # it is not an initialization
                end
            end

            func main
                f := new Foo
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
    }

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
