#if !defined DACHS_HELPER_COLORIZER_HPP_INCLUDED
#define      DACHS_HELPER_COLORIZER_HPP_INCLUDED

#include <map>

namespace dachs {
namespace helper {

// MAYBE TODO:
// background color of text, underline, bold and blink effects

template<class String>
class colorizer {
    bool enabled;

    enum class color {
        yellow,
        green,
        gray,
        red,
        cyan,
        purple,
        blue,
        none,
    };

    enum class brightness {
        light,
        dark,
    };

    unsigned int color_table(color const c) const noexcept
    {
        switch(c) {
        case color::gray:   return 90u;
        case color::red:    return 91u;
        case color::green:  return 92u;
        case color::yellow: return 93u;
        case color::blue:   return 94u;
        case color::purple: return 95u;
        case color::cyan:   return 96u;
        case color::none:
        default:            return 0u;
        }
    }

    std::string start_sequence(brightness const b, color const c) const noexcept {
        auto code = color_table(c);
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

    explicit colorizer(bool const colorful_output=true) noexcept
        : enabled(colorful_output)
    {}

    String yellow(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::yellow, target, end_seq, b);
    }
    String green(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::green, target, end_seq, b);
    }
    String gray(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::gray, target, end_seq, b);
    }
    String red(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::red, target, end_seq, b);
    }
    String cyan(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::cyan, target, end_seq, b);
    }
    String purple(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::purple, target, end_seq, b);
    }
    String blue(String const& target, bool const end_seq=true, brightness const b=brightness::light) const noexcept
    {
        return colorize(color::blue, target, end_seq, b);
    }

    String reset() const noexcept
    {
        return enabled ? start_sequence(brightness::light, color::none) : "";
    }
};

} // namespace helper
} // namespace dachs


#endif    // DACHS_HELPER_COLORIZER_HPP_INCLUDED
