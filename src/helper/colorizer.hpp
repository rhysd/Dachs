#if !defined DACHS_HELPER_COLORIZER_HPP_INCLUDED
#define      DACHS_HELPER_COLORIZER_HPP_INCLUDED

#include <map>

namespace dachs {
namespace helper {

template<class String>
class colorizer {
    bool enable;

    enum class color {
        yellow,
        green,
        gray,
        red,
        cyan,
        purple,
        blue,
    };

    enum class color_type {
        light,
        dark,
    };

    unsigned int color_table(color const c) const noexcept
    {
        switch(c) {
        case color::yellow: return 93u;
        case color::green:  return 92u;
        case color::gray:   return 90u;
        case color::red:    return 91u;
        case color::cyan:   return 96u;
        case color::purple: return 95u;
        case color::blue:   return 94u;
        }
    }

    std::string start_sequence(color_type const t, color const c) const noexcept {
        auto code = color_table(c);
        if(t == color_type::dark) code -= 60;
        return "\033[" + std::to_string(code) + 'm';
    };

    String colorize(color const c, String const& target, bool const end_sequence, color_type const t) const noexcept
    {
        if (enable) {
            return start_sequence(t, c) + target + (end_sequence ? "\033[0m" : "");
        } else {
            return target;
        }
    }

public:

    explicit colorizer(bool const colorful_output=true)
        : enable(colorful_output)
    {}

    String yellow(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::yellow, target, end_seq, type);
    }
    String green(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::green, target, end_seq, type);
    }
    String gray(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::gray, target, end_seq, type);
    }
    String red(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::red, target, end_seq, type);
    }
    String cyan(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::cyan, target, end_seq, type);
    }
    String purple(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::purple, target, end_seq, type);
    }
    String blue(String const& target, bool const end_seq=true, color_type const type=color_type::light) const noexcept
    {
        return colorize(color::blue, target, end_seq, type);
    }
};

} // namespace helper
} // namespace dachs


#endif    // DACHS_HELPER_COLORIZER_HPP_INCLUDED
