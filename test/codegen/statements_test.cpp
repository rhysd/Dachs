#define BOOST_TEST_MODULE LLVMCodegenStatementsTest
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

BOOST_AUTO_TEST_CASE(compound_assign)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            var i := 10
            i += 10
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

BOOST_AUTO_TEST_CASE(main_func)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main(args)
            argc := args.size

            var i := 0u
            for i < argc
                args[i].println
                i += 1u
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main : ()
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            ret 0
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main : int
            ret 0
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main(args)
            ret 0
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main(args) : int
            ret 0
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
