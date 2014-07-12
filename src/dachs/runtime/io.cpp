#include <string>
#include <cstdio>

extern "C" {
    void dachs_put_float__(double const d)
    {
        std::printf("%lf\n", d);
    }

    void dachs_put_int__(int const i)
    {
        std::printf("%d\n", i);
    }

    void dachs_put_uint__(unsigned int const u)
    {
        std::printf("%u\n", u);
    }

    void dachs_put_char__(char const c)
    {
        std::printf("%c\n", c);
    }

    void dachs_put_string__(char const* const s)
    {
        std::printf("%s\n", s);
    }

    void dachs_put_symbol__(char const* const s)
    {
        std::printf("%s\n", s);
    }

    void dachs_put_bool__(bool const b)
    {
        std::printf("%s\n", b ? "true" : "false");
    }

    void dachs_print_float__(double const d)
    {
        std::printf("%lf", d);
    }

    void dachs_print_int__(int const i)
    {
        std::printf("%d", i);
    }

    void dachs_print_uint__(unsigned int const u)
    {
        std::printf("%u", u);
    }

    void dachs_print_char__(char const c)
    {
        std::printf("%c", c);
    }

    void dachs_print_string__(char const* const s)
    {
        std::printf("%s", s);
    }

    void dachs_print_symbol__(char const* const s)
    {
        std::printf("%s", s);
    }

    void dachs_print_bool__(bool const b)
    {
        std::printf("%s", b ? "true" : "false");
    }
}
