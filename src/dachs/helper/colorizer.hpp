#if !defined DACHS_HELPER_COLORIZER_HPP_INCLUDED
#define      DACHS_HELPER_COLORIZER_HPP_INCLUDED

#include <map>
#include <string>

namespace dachs {
namespace helper {

// MAYBE TODO:
// background color of text, underline, bold and blink effects

template<class String>
class basic_colorizer {

    enum class color : unsigned int {
        gray = 90u,
        red = 91u,
        green = 92u,
        yellow = 93u,
        blue = 94u,
        purple = 95u,
        cyan = 96u,
        none = 0u,
    };

    enum class brightness {
        light,
        dark,
    };

    std::string start_sequence(brightness const b, color const c) const noexcept {
        auto code = static_cast<unsigned int>(c);
        if(b == brightness::dark && code >= 60u) {
            code -= 60u;
        }
        return "\033[" + std::to_string(code) + 'm';
    };

    String colorize(color const c, String const& target, bool const end_sequence, brightness const b) const noexcept
    {
        if (enabled) {
            return start_sequence(b, c) + target + (end_sequence ? "\033[0m" : "");
        } else {
            return target;
        }
    }

public:

#define DEFINE_COLORIZE(c) \
    String c(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept \
    { \
        return colorize(color::c, target, end_seq, b); \
    }
    DEFINE_COLORIZE(yellow)
    DEFINE_COLORIZE(green)
    DEFINE_COLORIZE(gray)
    DEFINE_COLORIZE(red)
    DEFINE_COLORIZE(cyan)
    DEFINE_COLORIZE(purple)
    DEFINE_COLORIZE(blue)
#undef DEFINE_COLORIZE

    String reset() const noexcept
    {
        return enabled ? start_sequence(brightness::light, color::none) : "";
    }

public:
    static bool enabled;

};

template<class String>
bool basic_colorizer<String>::enabled = true;

using colorizer = basic_colorizer<std::string>;

} // namespace helper
} // namespace dachs


#endif    // DACHS_HELPER_COLORIZER_HPP_INCLUDED
