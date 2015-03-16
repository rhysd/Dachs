#define BOOST_TEST_MODULE Helper
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include <type_traits>
#include <string>

#include <boost/test/included/unit_test.hpp>

#include "dachs/helper/probable.hpp"

using namespace dachs::helper;

BOOST_AUTO_TEST_SUITE(helper)

BOOST_AUTO_TEST_SUITE(helper)

BOOST_AUTO_TEST_CASE(string_error)
{
    enum class test {
        foo, bar, baz
    };

    static_assert(std::is_same<typename probable<int>::success_type, int>::value, "");
    static_assert(std::is_same<typename probable<int>::failure_type, std::string>::value, "");
    static_assert(std::is_same<typename probable<int, char>::failure_type, char>::value, "");

    auto const p1 = make_probable<test>(test::bar);
    BOOST_CHECK(p1.success());
    BOOST_CHECK(!p1.failure());

    auto const r1 = p1.get();
    BOOST_CHECK(r1);
    BOOST_CHECK(*r1 == test::bar);
    BOOST_CHECK(p1.get_unsafe() == test::bar);
    BOOST_CHECK(p1.raw_value().which() == 0u);

    auto const f1 = p1.get_error();
    BOOST_CHECK(!f1);

    probable<test> const p2 = oops("error occurred!");
    BOOST_CHECK(!p2.success());
    BOOST_CHECK(p2.failure());

    auto const r2 = p2.get();
    BOOST_CHECK(!r2);

    auto const f2 = p2.get_error();
    BOOST_CHECK(f2);
    BOOST_CHECK(*f2 == "error occurred!");
    BOOST_CHECK(p2.get_error_unsafe() == "error occurred!");
    BOOST_CHECK(p2.raw_value().which() == 1u);

    auto p3 = probably(test::baz);
    p3 = test::foo;
    auto foo = test::foo;
    p3 = foo;
    auto const foo2 = foo;
    p3 = foo2;

    auto p4 = p3;
    probable<test> p5 = test::foo;
    BOOST_CHECK(p5.success());

    BOOST_CHECK(p4 == p5);
    BOOST_CHECK(p1 != p5);
    BOOST_CHECK(p2 != p5);
    BOOST_CHECK(p2 == p2);
    BOOST_CHECK(p5 < p1);

    auto const g = make_probable_generator<test>();
    auto const p6 = g(test::foo);
    BOOST_CHECK(p6.success());

    probable<test> const p7 = oops_fmt("there is %1% errors", 7);
    BOOST_CHECK(p7.failure());
    BOOST_CHECK(*p7.get_error() == "there is 7 errors");
    oops_fmt("%1%, %2%, %3%", 1, 3.14, "aaa");
    oops_fmt("does not contain any formatters");
}

BOOST_AUTO_TEST_CASE(user_defined_error)
{
    enum class test {
        foo, bar, baz
    };

    enum class error {
        err1, err2, err3
    };

    using test_type = probable<test, error>;

    test_type const p1 = test::bar;
    BOOST_CHECK(p1.success());
    BOOST_CHECK(!p1.failure());

    auto const r1 = p1.get();
    BOOST_CHECK(r1);
    BOOST_CHECK(*r1 == test::bar);
    BOOST_CHECK(p1.get_unsafe() == test::bar);
    BOOST_CHECK(p1.raw_value().which() == 0u);

    auto const f1 = p1.get_error();
    BOOST_CHECK(!f1);

    test_type const p2 = oops<error>(error::err1);
    BOOST_CHECK(!p2.success());
    BOOST_CHECK(p2.failure());

    auto const r2 = p2.get();
    BOOST_CHECK(!r2);

    auto const f2 = p2.get_error();
    BOOST_CHECK(f2);
    BOOST_CHECK(*f2 == error::err1);
    BOOST_CHECK(p2.get_error_unsafe() == error::err1);
    BOOST_CHECK(p2.raw_value().which() == 1u);
}

BOOST_AUTO_TEST_CASE(edge_case_string)
{
    auto const p = make_probable<std::string>("aaaa");
    BOOST_CHECK(p.success());
    auto const r = p.get();
    BOOST_CHECK(r);
    BOOST_CHECK(*r == "aaaa");
    auto const v = p.get_value_or_error();

    probable<std::string> const p2 = oops("bbbb");
    BOOST_CHECK(p2.failure());
    auto const f = p2.get_error();
    BOOST_CHECK(f);
    BOOST_CHECK(*f == "bbbb");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
