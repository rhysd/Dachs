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
    colorizer c;

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

    std::string demangle(char const* name) const noexcept
    {
#if BOOST_VERSION >= 105600
        return boost::core::demangle(name);
#else
        return name;
#endif
    }

    backtrace_frame parse_frame_libstdcpp(char const* const frame) const
    {
        std::string detail = frame;
        char const* start;
        char const* last;
        start = last = frame;

        auto const advance_last_until
            = [&](char const c)
            {
                while(*last != c && *last != '\0') ++last;
                return *last != '\0';
            };

        auto const tokenize_until
            = [&](char const c) -> std::string
            {
                if (advance_last_until(c)) {
                    auto const s = start, l = last;
                    start = ++last;
                    return {s, l};
                } else {
                    return "UNKNOWN";
                }
            };

        std::string const objname = tokenize_until('(');
        std::string const demangled = demangle(tokenize_until(')').c_str());

        if (advance_last_until('[')) {
            start = ++last;
        }

        std::string const address = tokenize_until(']');

        return {objname, address, demangled, detail};
    }

    backtrace_frame parse_frame_libcxx(char const* const frame) const
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
        auto const objname = get_token();

        auto const address = get_token();
        skip_white();
        std::string detail = current;

        return {objname, address, demangle(get_token().c_str()), detail};
    }

    backtrace_frame parse_frame(char const* const frame) const
    {
#if defined(__GLIBCPP__) || defined(__GLIBCXX__)
        return parse_frame_libstdcpp(frame);
#else
        return parse_frame_libcxx(frame);
#endif
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
        out << "###################\n"
            << "#    Backtrace    #\n"
            << "###################\n";
        each_frame_with_index(
            [&](std::size_t const i, auto const& f)
            {
                out << '[' << i << "] " << f.demangled << '\n'
                    << "  " << "object: " << f.object << " (" << f.address << ')' << '\n'
                    << "  " << "detail: " << f.detail << '\n';
            }
        );
    }

    template<class Out = std::ostream>
    void dump_pretty_backtrace(Out &out = std::cerr) const
    {
        out << "###################\n"
            << "#    Backtrace    #\n"
            << "###################\n";
        each_frame_with_index(
            [&](std::size_t const i, auto const& f)
            {
                out << c.green('[' + std::to_string(i) + "] " + f.demangled) << '\n'
                    << "  " << c.yellow("object: ") << f.object << " (" << f.address << ')' << '\n'
                    << "  " << c.yellow("detail: ") << f.detail << '\n';
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
