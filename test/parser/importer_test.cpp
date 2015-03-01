#define BOOST_TEST_MODULE ParserTest
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include "../test_helper.hpp"

#include <string>
#include <vector>

#include <boost/test/included/unit_test.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/exception.hpp"


using namespace dachs::test;

static dachs::syntax::parser p;
static std::vector<std::string> importdirs = {DACHS_ROOT_DIR "/test/assets/import_test"};
static std::string dummy_file = DACHS_ROOT_DIR "/test/assets/import_test/dummy/dummy.dcs";

#define CHECK_THROW_IMPORT(...) \
        BOOST_CHECK_THROW( \
            dachs::syntax::import( \
                p.parse((__VA_ARGS__), dummy_file), \
                importdirs, \
                p, \
                dummy_file \
            ), \
            dachs::parse_error \
        )

#define CHECK_NO_THROW_IMPORT(...) \
        BOOST_CHECK_NO_THROW( \
            dachs::syntax::import( \
                p.parse((__VA_ARGS__), dummy_file), \
                importdirs, \
                p, \
                dummy_file \
            ) \
        )

BOOST_AUTO_TEST_SUITE(importer)

BOOST_AUTO_TEST_CASE(normal_cases)
{
    CHECK_NO_THROW_IMPORT("import std.range");
    CHECK_NO_THROW_IMPORT("import foo");
    CHECK_NO_THROW_IMPORT("import bar");
    CHECK_NO_THROW_IMPORT("import foo.aaa");
    CHECK_NO_THROW_IMPORT("import foo.bbb");
    CHECK_NO_THROW_IMPORT("import relative_path_test");

    CHECK_NO_THROW_IMPORT(R"(
        import std.range
        import foo
        import bar
        import foo.aaa
        import foo.bbb
    )");

    CHECK_NO_THROW_IMPORT("import self_import");
}

BOOST_AUTO_TEST_CASE(abnormal_cases)
{
    CHECK_THROW_IMPORT("import unknown_file");
    CHECK_THROW_IMPORT("import foo.moudame");
    CHECK_THROW_IMPORT("import error1");
}

BOOST_AUTO_TEST_SUITE_END()

