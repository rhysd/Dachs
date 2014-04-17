#include <exception>
#include <iostream>
#include <iterator>
#include <fstream>

#include <boost/optional.hpp>

#include "dachs.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {

template <class String>
inline boost::optional<String> read_file(std::string const& file_name)
{
    typedef typename String::value_type CharT;

    std::basic_ifstream<CharT> input(file_name, std::ios::in);
    if (!input.is_open()) {
        return boost::none;
    }
    return String{std::istreambuf_iterator<CharT>{input},
                  std::istreambuf_iterator<CharT>{}};
}

}  // namespace dachs

int main(int const argc, char const* const argv[])
{
    // TODO: use Boost.ProgramOptions
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " {file}\n";
        return 1;
    }

    auto const maybe_code = dachs::read_file<std::string>(argv[1]);
    if (!maybe_code) {
        std::cerr << "File cannot be opened: " << argv[1] << std::endl;
        return 2;
    }
    auto const& code = *maybe_code;
    dachs::helper::colorizer<std::string> c;

    dachs::compiler compiler;
    try {
        std::cout << compiler.compile(code) << std::endl
                  << c.blue("〜完〜") << std::endl;
    }
    catch (dachs::syntax::parse_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 4;
    }
    catch (std::exception const& e) {
        std::cerr << "Internal compilation error: " << e.what() << std::endl;
        return 3;
    }

    return 0;
}