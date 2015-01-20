#define BOOST_TEST_MODULE LLVMCodegenClassTest
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
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()

