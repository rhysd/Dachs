#if !defined DACHS_HELPER_COLORIZER_HPP_INCLUDED
#define      DACHS_HELPER_COLORIZER_HPP_INCLUDED

#include <map>
#include <string>

#include <boost/format.hpp>

namespace dachs {
namespace helper {

// MAYBE TODO:
// background color of text, underline, bold and blink effects

template<class String>
class basic_colorizer {
public:

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

    enum class attr : unsigned int {
        bold = 1u,
        underscore = 4u,
        blink = 5u,
        reverse = 7u,
        concealed = 8u,
        none = 0u,
    };

private:

    template<class Code>
    std::string seq(Code const code) const noexcept
    {
        return "\033[" + std::to_string(static_cast<unsigned int>(code)) + 'm';
    }

    std::string start_sequence(brightness const b, color const c) const noexcept
    {
        auto code = static_cast<unsigned int>(c);
        if(b == brightness::dark && code >= 60u) {
            code -= 60u;
        }
        return seq(code);
    };

    String build_sequence(color const c, String const& raw, bool const ends_seq, brightness const b, attr const a) const noexcept
    {
        std::string result = start_sequence(b, c);
        if (a != attr::none) {
            result += seq(a);
        }
        result += raw;
        if (ends_seq) {
            result += seq(color::none);
        }
        return result;
    }

    String colorize(color const c, String const& raw, bool const ends_seq, brightness const b, attr const a) const noexcept
    {
        if (enabled) {
            return build_sequence(c, raw, ends_seq, b, a);
        } else {
            return raw;
        }
    }

    String colorize(color const c, boost::format const& fmt, bool const ends_seq, brightness const b, attr const a) const noexcept
    {
        return colorize(c, fmt.str(), ends_seq, b, a);
    }

    String attribute(attr const a, String const& raw, bool const ends_seq) const noexcept
    {
        if (enabled) {
            return seq(a) + raw + (ends_seq ? seq(attr::none) : "");
        } else {
            return raw;
        }
    }

    String attribute(attr const a, boost::format const& fmt, bool const ends_seq) const noexcept
    {
        return attribute(a, fmt.str(), ends_seq);
    }

public:

#define DACHS_COLORIZER_DEFINE_COLORIZE(c) \
    template<class S> \
    String c(S const& target, bool const end_seq=true, attr const a=attr::none, brightness const b=brightness::light) const noexcept \
    { \
        return colorize(color::c, target, end_seq, b, a); \
    }
    DACHS_COLORIZER_DEFINE_COLORIZE(yellow)
    DACHS_COLORIZER_DEFINE_COLORIZE(green)
    DACHS_COLORIZER_DEFINE_COLORIZE(gray)
    DACHS_COLORIZER_DEFINE_COLORIZE(red)
    DACHS_COLORIZER_DEFINE_COLORIZE(cyan)
    DACHS_COLORIZER_DEFINE_COLORIZE(purple)
    DACHS_COLORIZER_DEFINE_COLORIZE(blue)

#define DACHS_COLORIZER_DEFINE_ATTR(a) \
    template<class S> \
    String a(S const& target, bool const ends_seq=true) const noexcept \
    { \
        return attribute(attr::a, target, ends_seq); \
    }
    DACHS_COLORIZER_DEFINE_ATTR(bold)
    DACHS_COLORIZER_DEFINE_ATTR(underscore)
    DACHS_COLORIZER_DEFINE_ATTR(blink)
    DACHS_COLORIZER_DEFINE_ATTR(reverse)
    DACHS_COLORIZER_DEFINE_ATTR(concealed)

#undef DACHS_COLORIZER_DEFINE_ATTR
#undef DACHS_COLORIZER_DEFINE_COLORIZE

    String reset() const noexcept
    {
        return enabled ? seq(color::none) : "";
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
