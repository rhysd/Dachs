#define BOOST_TEST_MODULE ParserTest

#include <string>
#include <exception>

#include "dachs/parser.hpp"
#include "dachs/helper/util.hpp"

#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <boost/range.hpp>

namespace fs = boost::filesystem;

inline
auto traverse_directory_range(std::string const& dir_name)
{
    fs::path const p = dir_name;
    return boost::make_iterator_range(fs::directory_iterator{p}, fs::directory_iterator{});
}

BOOST_AUTO_TEST_SUITE(parser)
BOOST_AUTO_TEST_CASE(function)
{
    dachs::syntax::parser parser;

    for( fs::path const& p : traverse_directory_range("assets/function") ) {
        BOOST_CHECK_NO_THROW(parser.parse(*dachs::helper::read_file<std::string>(p.c_str())));
    }
}

BOOST_AUTO_TEST_CASE(procedure)
{
    dachs::syntax::parser parser;

    for( fs::path const& p : traverse_directory_range("assets/procedure") ) {
        BOOST_CHECK_NO_THROW(parser.parse(*dachs::helper::read_file<std::string>(p.c_str())));
    }

    // procedure doesn't have return type
    BOOST_CHECK_THROW(parser.parse(R"(proc hoge : int; end)"), dachs::syntax::parse_error);
}

BOOST_AUTO_TEST_SUITE_END()
