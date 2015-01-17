#define BOOST_TEST_MODULE LLVMCodegenSamplesTest
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
BOOST_AUTO_TEST_SUITE(samples)

BOOST_AUTO_TEST_CASE(samples)
{
    auto const dir = DACHS_ROOT_DIR "/test/assets/samples";
    check_all_cases_in_directory(dir, [](fs::path const& path){
                std::cout << "testing " << path.c_str() << std::endl;
                CHECK_NO_THROW_CODEGEN_ERROR(
                    *dachs::helper::read_file<std::string>(path.c_str())
                );
            });
}

BOOST_AUTO_TEST_CASE(some_samples)
{
    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            print("Hello, Dachs!\n")
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func new_year(y)
            "A happy new year ".print
            y.println
        end

        func main
            2015.new_year
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func fib(n)
            case n
            when 0, 1
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n : float) : float
            ret (if n > 0.0 then n else -n)
        end

        func sqrt(x : float) : float
            var z := x
            var p := 0.0
            for abs(p-z) > 0.00001
                p, z = z, z-(z*z-x)/(2.0*z)
            end
            ret z
        end

        func main
            print(sqrt(10.0))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func abs(n)
            ret (if n > 0.0 then n else -n)
        end

        func sqrt'(p, z, x)
            ret z if abs(p-z) < 0.00001
            ret sqrt'(z, z-(z*z-x)/(2.0*z), x)
        end

        func sqrt(x)
            ret sqrt'(0.0, x, x)
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
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func fib(n)
            case n
            when 0, 1
                ret 1
            else
                ret fib(n-1) + fib(n-2)
            end
        end

        func main()
            print(fib(10))
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func map(var a, p)
            var i := 0u
            for i < a.size
                a[i] = p(a[i])
                i += 1u
            end

            ret a
        end

        func square(x)
            ret x * x
        end

        func main
            a := [1, 2, 3, 4, 5]
            a2 := map(a, square)
            for e in a2
                println(e)
            end

            var a3 := [1.1, 2.2, 3.3, 4.4, 5.5]
            var a4 := map(a3, square)
            for e in a4
                println(e)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func map(var a, p)
            var i := 0u
            for i < a.size
                a[i] = p(a[i])
                i += 1u
            end

            ret a
        end

        func each(a, p)
            for e in a
                p(e)
            end
        end

        func square(x)
            ret x * x
        end

        func main
            [1, 2, 3, 4, 5].map(square).each println
            [1.1, 2.2, 3.3, 4.4, 5.5].map(square).each println
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func each(a, p)
            for e in a
                p(e)
            end
        end

        func main
            [1, 2, 3, 4].each do |i|
                println(i)
            end
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            f := -> x in x + 1
            f2 := -> 42
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
