#if !defined DACHS_HELPER_BACKTRACE_PRINTER_HPP_INCLUDED
#define      DACHS_HELPER_BACKTRACE_PRINTER_HPP_INCLUDED

#include <iostream>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdlib>

#include <execinfo.h>

#include <boost/version.hpp>
#if BOOST_VERSION >= 105600
#   include <boost/core/demangle.hpp>
#endif

#include "dachs/helper/colorizer.hpp"

namespace dachs {
namespace helper {

template<std::size_t MaxFrames = 100u, class String = std::string>
struct backtrace_printer {
    colorizer<String> c;

    struct backtrace_frame {
        std::string object;
        std::string address;
        std::string demangled;
        std::string detail;
    };

    explicit backtrace_printer(decltype(c) const& c)
        : c(c)
    {
        void *buffer[MaxFrames];

        frame_size = backtrace(buffer, MaxFrames);
        frames = backtrace_symbols(buffer, frame_size);
    }

    ~backtrace_printer()
    {
        std::free(frames);
    }

    backtrace_frame parse_frame(char const* const frame) const
    {
        char const* current = frame;

        auto const skip_white
            = [&]
            {
                while(*current == ' ' && *current != '\0') ++current;
            };

        auto const get_token
            = [&]
            {
                skip_white();

                std::string result;

                while(*current != ' ' && *current != '\0') {
                    result.push_back(*current);
                    ++current;
                }

                return result;
            };

        get_token(); // Skip no.
        auto const libname = get_token();

        auto const address = get_token();
        skip_white();
        std::string detail = current;

#if BOOST_VERSION >= 105600
        auto const demangled = boost::core::demangle(get_token());
#else
        auto const demangled = get_token();
#endif

        return {libname, address, demangled, detail};
    }


    template<class Predicate>
    void each_frame(Predicate &&pred) const
    {
        for (std::size_t i = 0u; i < frame_size; ++i) {
            pred(parse_frame(frames[i]));
        }
    }

    template<class Predicate>
    void each_frame_with_index(Predicate &&pred) const
    {
        for (std::size_t i = 0u; i < frame_size; ++i) {
            pred(i, parse_frame(frames[i]));
        }
    }

    // Note:
    // Do not use 'auto' here because debug information for it
    // is not implemented in clang.
    template<class Result = std::vector<backtrace_frame>>
    Result get_backtrace() const
    {
        Result result;
        result.reserve(frame_size);

        each_frame([&](auto const& f){ result.emplace_back(f); });

        return result;
    }

    template<class Out = std::ostream>
    void dump_backtrace(Out &out = std::cerr) const
    {
        each_frame_with_index(
            [&](std::size_t const i, auto const& f)
            {
                out << '[' << i << "] " << f.demangled << '\n'
                    << "  " << f.object << " (" << f.address << ')' << '\n'
                    << "  " << f.detail << '\n';
            }
        );
    }

    template<class Out = std::ostream>
    void dump_pretty_backtrace(Out &out = std::cerr) const
    {
        each_frame_with_index(
            [&](std::size_t const i, auto const& f)
            {
                out << c.green('[' + std::to_string(i) + "] " + f.demangled) << '\n'
                    << "  " << f.object << " (" << f.address << ')' << '\n'
                    << "  " << f.detail << '\n';
            }
        );
    }


private:

    char **frames = nullptr;
    std::size_t frame_size = 0u;
};

} // namespace helper
} // namespace dachs

#endif    // DACHS_HELPER_BACKTRACE_PRINTER_HPP_INCLUDED
