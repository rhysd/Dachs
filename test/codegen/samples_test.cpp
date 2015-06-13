#define BOOST_TEST_MODULE LLVMCodegenSamplesTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"
#include "./codegen_test_helper.hpp"

using namespace dachs::test;

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
            ret if n > 0.0 then n else -n end
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
            ret if n > 0.0 then n else -n end
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

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func main
            a := [1, 2, 3, 4, 5]

            sum :=
                let
                    var s := 0
                in begin
                    for e in a
                        s += e
                    end
                    s
                end

            sum2 :=
                begin
                    var s := 0
                    for e in a
                        s += e
                    end
                    s
                end

            sum3 :=
                let
                    var s := 0
                in begin
                    for e in a
                        s += e
                    end
                    s
                end

            sum4 :=
                let
                    var s := 0
                in begin
                    for e in a
                        s += e
                    end
                    s
                end

            let
                var s := 1
                var result : int
            in begin
                case s
                when 0
                    result = sum
                when 1
                    result = sum2
                when 2
                    result = sum3
                when 3
                    result = sum4
                end
                result
            end.println

            println(sum == sum2)
            println(sum == sum3)
            println(sum == sum4)
        end
    )");

    CHECK_NO_THROW_CODEGEN_ERROR(R"(
        func test(b : bool)
            print(if b then '.' else 'F' end)
        end

        func test_not(b : bool)
            test(!b)
        end

        func main
            test("abc" < "def")
            test("abc" < "abcdef")
            test_not("def" < "abc")
            test_not("abcdef" < "abc")
            test_not("abc" < "abc")

            test("abc" == "abc")
            test_not("abc" == "bcd")
            test_not("abc" == "abcd")
            test("" == "")
            test_not("aaa" == "")

            test("abcdef".start_with? "abc")
            test("abcdef".start_with? "abcdef")
            test_not("abc".start_with? "abcdef")
            test_not("def".start_with? "abc")
            test_not("def".start_with? "abcdef")
            test("def".start_with? "")

            test("abcdef".end_with? "def")
            test("abcdef".end_with? "abcdef")
            test_not("abc".end_with? "abcdef")
            test_not("abc".end_with? "def")
            test_not("abc".end_with? "abcdef")
            test("def".end_with? "")

            s := new string
            test(s == "")
        end
    )");
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
