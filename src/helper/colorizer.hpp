#if !defined DACHS_HELPER_COLORIZER_HPP_INCLUDED
#define      DACHS_HELPER_COLORIZER_HPP_INCLUDED

namespace dachs {
namespace helper {

template<class String>
class colorizer {
    bool enable;

    String colorize(char const* const sequence, String const& target, bool const end_sequence) const noexcept
    {
        if (enable) {
            return sequence + target + (end_sequence ? "\033[0m" : "");
        } else {
            return target;
        }
    }
public:

    explicit colorizer(bool const colorful_output=true)
        : enable(colorful_output)
    {}

    String yellow(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[93m", target, end_seq);
    }
    String green(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[92m", target, end_seq);
    }
    String gray(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[90m", target, end_seq);
    }
    String red(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[91m", target, end_seq);
    }
    String cyan(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[96m", target, end_seq);
    }
    String purple(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[95m", target, end_seq);
    }
    String blue(String const& target, bool const end_seq=true) const noexcept
    {
        return colorize("\033[94m", target, end_seq);
    }
};

} // namespace helper
} // namespace dachs


#endif    // DACHS_HELPER_COLORIZER_HPP_INCLUDED
