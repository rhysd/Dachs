#if !defined DACHS_HELPER_BACKTRACE_PRINTER_HPP_INCLUDED
#define      DACHS_HELPER_BACKTRACE_PRINTER_HPP_INCLUDED

#include <iostream>
#include <string>
#include <vector>

#include <cstddef>
#include <cstdlib>

#include <execinfo.h>

#include "dachs/helper/colorizer.hpp"

namespace dachs {
namespace helper {

template<std::size_t MaxFrames = 100u, class String = std::string>
struct backtrace_printer {
    colorizer<String> c;

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

    template<class Predicate>
    void each_frame(Predicate &&pred) const
    {
        for (std::size_t i = 0u; i < frame_size; ++i) {
            pred(frames[i]);
        }
    }

    // Note:
    // Do not use 'auto' here because debug information for it
    // is not implemented in clang.
    template<class Result = std::vector<std::string>>
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
        each_frame([&](auto const& f){ out << f << '\n'; });
    }

    template<class Out = std::ostream>
    void dump_pretty_backtrace(Out &out = std::cerr) const
    {
        bool is_even_line = false;

        each_frame(
            [&](auto const& f)
            {
                out << (is_even_line ? c.gray(f) : f)
                    << '\n';

                is_even_line = !is_even_line;
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
