#include <exception>
#include <iostream>

#include "dachs/compiler.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/exception.hpp"

int main(int const argc, char const* const argv[])
{
    // TODO: Use Boost.ProgramOptions
    //  --dump-ast
    //  --dump-sym-table
    //  --emit-llvm
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " {file}\n";
        return 1;
    }

    auto const maybe_code = dachs::helper::read_file<std::string>(argv[1]);
    if (!maybe_code) {
        std::cerr << "File cannot be opened: " << argv[1] << std::endl;
        return 2;
    }
    auto const& code = *maybe_code;
    dachs::helper::colorizer<std::string> c;

    dachs::compiler compiler;
    try {
        compiler.compile(code);
        std::cout << c.blue("〜完〜") << std::endl;
    }
    catch (dachs::parse_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 3;
    }
    catch (dachs::semantic_check_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 4;
    }
    catch (dachs::code_generation_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
    }
    catch (dachs::not_implemented_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 5;
    }
    catch (std::exception const& e) {
        std::cerr << "Internal compilation error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
