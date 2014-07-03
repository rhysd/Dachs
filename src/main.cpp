#include <exception>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include "dachs/compiler.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/exception.hpp"

template<class Action>
int do_compiler_action(char const* const file, Action const& action)
{

    if (!boost::algorithm::ends_with(file, ".dcs")) {
        std::cerr << "File's extension must be '.dcs'." << std::endl;
        return 2;
    }

    auto const maybe_code = dachs::helper::read_file<std::string>(file);
    if (!maybe_code) {
        std::cerr << "File cannot be opened: " << file << std::endl;
        return 3;
    }
    auto const& code = *maybe_code;
    dachs::helper::colorizer<std::string> c;

    try {
        action(file, code);
        std::cout << c.blue("〜完〜") << std::endl;
        return 0;
    }
    catch (dachs::parse_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 10;
    }
    catch (dachs::semantic_check_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 11;
    }
    catch (dachs::code_generation_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 12;
    }
    catch (dachs::not_implemented_error const& e) {
        std::cerr << e.what() << std::endl
                  << c.red("〜完〜") << std::endl;
        return 13;
    }
    catch (std::exception const& e) {
        std::cerr << "Internal compilation error: " << e.what() << std::endl;
        return -1;
    }
}

std::vector<char const*> filter_libdir(char const* const* arg, std::vector<std::string> &libdirs)
{
    std::vector<char const*> args;
    for (; *arg; ++arg) {
        if (boost::algorithm::starts_with(*arg, "--libdir=")) {
            auto libs_str = std::string{*arg}.substr(std::strlen("--libdir="));
            std::vector<std::string> libs;
            boost::algorithm::split(
                    libs,
                    libs_str,
                    boost::is_any_of(",")
                );
            libdirs.insert(std::end(libdirs), std::begin(libs), std::end(libs));
        } else {
            args.emplace_back(*arg);
        }
    }
    return args;
}

int main(int const, char const* const argv[])
{
    auto const show_usage =
        [argv]()
        {
            std::cerr << "Usage: " << argv[0] << " [--dump-ast|--dump-sym-table|--emit-llvm] [--libdir={path}] {file}\n";
        };

    // TODO: Use Boost.ProgramOptions

    std::vector<std::string> libdirs;
    std::vector<char const*> args = filter_libdir(&argv[0], libdirs);

    switch (args.size()) {

        dachs::compiler compiler;

    case 3: {
        std::string const opt = args[1];

        if (opt == "--dump-ast") {
            return do_compiler_action(
                    args[2],
                    [&](auto const&, auto const& s){ std::cout << compiler.dump_ast(s, true) << std::endl; }
                );
        } else if (opt == "--dump-sym-table") {
            return do_compiler_action(
                    args[2],
                    [&](auto const&, auto const& s){ std::cout << compiler.dump_scopes(s) << std::endl; }
                );
        } else if (opt == "--emit-llvm") {
            return do_compiler_action(
                    args[2],
                    [&](auto const&, auto const& s){ std::cout << compiler.dump_llvm_ir(s) << std::endl; }
                );
        } else {
            show_usage();
            return 1;
        }
    }

    case 2:
        return do_compiler_action(
                args[1],
                [&](auto const& f, auto const& s){ compiler.compile(f, s, libdirs); }
            );

    default:
        show_usage();
        return 1;
    }
}

