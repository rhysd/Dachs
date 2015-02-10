#define BOOST_TEST_MODULE Runtime
#define BOOST_DYN_LINK
#define BOOST_TEST_MAIN

#include <random>
#include <string>

#include <boost/test/included/unit_test.hpp>

#include "dachs/runtime/runtime.hpp"

std::mt19937 random_engine{std::random_device{}()};

inline std::string generate_random_string()
{
    std::size_t const size = std::uniform_int_distribution<std::size_t>(0, 256)(random_engine);

    std::string result;
    result.reserve(size);

    std::uniform_int_distribution<char> d(33, 126); // Random printable characters
    for (auto i = 0u; i < size; ++i) {
        result.push_back(d(random_engine));
    }

    return result;
}

inline std::uint64_t hash_of(std::string const& s)
{
    return dachs::runtime::cityhash64<std::uint64_t>{}(s.data(), s.size());
}

BOOST_AUTO_TEST_SUITE(runtime)

BOOST_AUTO_TEST_CASE(cityhash64)
{
    for (auto i = 0u; i < 1000u; ++i) {
        auto const s = generate_random_string();
        BOOST_CHECK(hash_of(s) == hash_of(s));
    }

    for (auto i = 0u; i < 1000u; ++i) {
        auto const l = generate_random_string();
        auto const r = generate_random_string();

        if (l != r) {
            BOOST_CHECK(hash_of(l) != hash_of(r));
        } else {
            BOOST_CHECK(hash_of(l) == hash_of(r));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

