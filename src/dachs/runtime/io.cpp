#include <string>
#include <cstdio>

extern "C" {
    void dachs_put_float__(double const d)
    {
        std::puts(std::to_string(d).c_str());
    }

    void dachs_put_int__(int const i)
    {
        std::puts(std::to_string(i).c_str());
    }

    void dachs_put_uint__(unsigned int const u)
    {
        std::puts(std::to_string(u).c_str());
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

    void dachs_print_string__(char *s)
    {
        std::printf(s);
    }
}
