#include <exception>
#include <iostream>

#include "dachs/compiler.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/exception.hpp"

template<class Action>
int do_compiler_action(char const* const file, Action const& action)
{
    auto const maybe_code = dachs::helper::read_file<std::string>(file);
    if (!maybe_code) {
        std::cerr << "File cannot be opened: " << file << std::endl;
        return 2;
    }
    auto const& code = *maybe_code;
    dachs::helper::colorizer<std::string> c;

    try {
        action(code);
        std::cout << c.blue("〜完〜") << std::endl;
        return 0;
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
        return 5;
    }
    catch (dachs::not_implemented_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 6;
    }
    catch (std::exception const& e) {
        std::cerr << "Internal compilation error: " << e.what() << std::endl;
        return -1;
    }
}

int main(int const argc, char const* const argv[])
{
    auto const show_usage =
        [=]()
        {
            std::cerr << "Usage: " << argv[0] << " [--dump-ast|--dump-sym-table|--emit-llvm] {file}\n";
        };

    // TODO: Use Boost.ProgramOptions

    switch (argc) {
        dachs::compiler compiler;

    case 3: {
        std::string const opt = argv[1];

        if (opt == "--dump-ast") {
            return do_compiler_action(
                    argv[2],
                    [&](auto const& s){ std::cout << compiler.dump_ast(s, true); }
                );
        } else if (opt == "--dump-sym-table") {
            return do_compiler_action(
                    argv[2],
                    [&](auto const& s){ std::cout << compiler.dump_scopes(s); }
                );
        } else if (opt == "--emit-llvm") {
            return do_compiler_action(
                    argv[2],
                    [&](auto const& s){ std::cout << compiler.dump_llvm_ir(s); }
                );
        } else {
            show_usage();
            return 1;
        }
    }

    case 2:
        return do_compiler_action(
                argv[1],
                [&](auto const& s){ compiler.compile(s); }
            );

    default:
        show_usage();
        return 1;
    }
}

