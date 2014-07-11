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
#include "dachs/exception.hpp"

namespace dachs {
namespace cmdline {

template<class Action>
int do_compiler_action(Action const& action)
{
    dachs::helper::colorizer<std::string> c;

    try {
        action();
        return 0;
    }
    catch (std::runtime_error const& e) {
        std::cerr << e.what() << std::endl;
        return 3;
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

template<class ArgPtr>
auto get_command_options(ArgPtr arg)
{
    struct {
        std::vector<char const*> rest_args;
        std::vector<std::string> source_files;
        std::vector<std::string> libdirs;
        bool debug = false;
        bool enable_color = true; 
    } cmdopts;

    std::string const debug_str = "--debug";
    std::string const disable_color_str = "--disable_color";

    for (; *arg; ++arg) {
        if (boost::algorithm::starts_with(*arg, "--libdir=")) {
            auto libs_str = std::string{*arg}.substr(std::strlen("--libdir="));
            std::vector<std::string> libs;
            boost::algorithm::split(
                    libs,
                    libs_str,
                    boost::is_any_of(",")
                );
            cmdopts.libdirs.insert(std::end(cmdopts.libdirs), std::begin(libs), std::end(libs));
        } else if (boost::algorithm::ends_with(*arg, ".dcs")) {
            cmdopts.source_files.emplace_back(*arg);
        } else if (*arg == debug_str) {
            cmdopts.debug = true;
        } else if (*arg == disable_color_str) {
            cmdopts.enable_color = false;
        } else {
            cmdopts.rest_args.emplace_back(*arg);
        }
    }

    return cmdopts;
}

} // namespace cmdline
} // namespace dachs

int main(int const, char const* const argv[])
{
    (U'ω') /* Hello, Dachs! */;

    auto const show_usage =
        [argv]()
        {
            std::cerr << "Usage: " << argv[0] << " [--dump-ast|--dump-sym-table|--emit-llvm|--output-obj] [--debug] [--libdir={path}] {file}\n";
        };

    // TODO: Use Boost.ProgramOptions

    auto const cmdopts = dachs::cmdline::get_command_options(&argv[1]);
    dachs::compiler compiler;

    switch (cmdopts.rest_args.size()) {

    case 1: {
        std::string const opt = cmdopts.rest_args[0];

        if (opt == "--dump-ast") {
            return dachs::cmdline::do_compiler_action(
                [&]{ compiler.dump_asts(std::cout, cmdopts.source_files, cmdopts.enable_color); }
            );
        } else if (opt == "--dump-sym-table") {
            return dachs::cmdline::do_compiler_action(
                [&]{ compiler.dump_scope_trees(std::cout, cmdopts.source_files); }
            );
        } else if (opt == "--emit-llvm") {
            return dachs::cmdline::do_compiler_action(
                [&]{ compiler.dump_llvm_irs(std::cout, cmdopts.source_files); }
            );
        } else if (opt == "--output-obj") {
            return dachs::cmdline::do_compiler_action(
                [&]{ compiler.compile_to_objects(cmdopts.source_files, cmdopts.enable_color, cmdopts.debug); }
            );
        } else {
            show_usage();
            return 1;
        }
    }

    case 0:
        return dachs::cmdline::do_compiler_action(
            [&]{ compiler.compile(cmdopts.source_files, cmdopts.libdirs, cmdopts.enable_color, cmdopts.debug); }
        );

    default:
        show_usage();
        return 1;
    }
}

