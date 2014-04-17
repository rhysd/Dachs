#define BOOST_TEST_MODULE ParserTest

#include <string>

#include "dachs/parser.hpp"

#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(empty)
{
    std::string const s = R"(func main; end)";
    dachs::syntax::parser p;
    BOOST_CHECK_NO_THROW(p.parse(s));
}
BOOST_AUTO_TEST_SUITE_END()
