#define BOOST_TEST_MODULE AnalyzerTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/exception.hpp"

#include <string>

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
BOOST_AUTO_TEST_SUITE(forward_analysis)

BOOST_AUTO_TEST_CASE(symbol_duplication_ok)
{
    CHECK_NO_THROW_SEMANTIC_ERROR(R"(
            # functions
            func foo
            end

            func foo'
            end

            func foo? : bool
                return true
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

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func foo(a)
                a := 1
            end

            func main
            end
        )");

    CHECK_THROW_SEMANTIC_ERROR(R"(
            # local variables
            func foo(a)
                var a : int
            end

            func main
            end
        )");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
