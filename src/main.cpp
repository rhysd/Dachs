#include <exception>
#include <iostream>
#include <vector>
#include <string>
#include <iterator>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include <signal.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dachs/compiler.hpp"
#include "dachs/helper/colorizer.hpp"
#include "dachs/helper/backtrace_printer.hpp"
#include "dachs/exception.hpp"
#include "dachs/codegen/opt_level.hpp"

namespace dachs {
namespace cmdline {

[[noreturn]]
static void signal_handler(int s)
{
    dachs::helper::colorizer c;
    helper::backtrace_printer<20u> printer{c};

    std::cerr << c.red("Caught deadly signal " + std::to_string(s)) << "\n\n";
    printer.dump_pretty_backtrace();

    std::cerr << std::endl;

    struct sigaction restored;
    sigemptyset(&restored.sa_mask);
    sigaddset(&restored.sa_mask, s);
    restored.sa_flags = SA_RESETHAND;

    sigaction(s, &restored, nullptr);
    raise(s);
    std::exit(1);
}

template<class Action>
int do_compiler_action(Action const& action)
{
    dachs::helper::colorizer c;

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

auto get_substitution_option(std::string const& s, char const* const prefix)
{
    auto const value_str = s.substr(std::strlen(prefix));
    std::vector<std::string> result;
    boost::algorithm::split(
            result,
            value_str,
            boost::is_any_of(",")
        );
    return result;
}

template<class T>
auto &operator+=(std::vector<T> &lhs, std::vector<T> &&rhs)
{
    lhs.insert(
            std::end(lhs),
            std::make_move_iterator(std::begin(rhs)),
            std::make_move_iterator(std::end(rhs))
        );
    return lhs;
}

template<class ArgPtr>
auto parse_command_options(ArgPtr arg)
{
    struct {
        std::vector<char const*> rest_args;
        std::vector<std::string> source_files;
        std::vector<std::string> libdirs;
        bool debug_compiler = false;
        bool enable_color = true;
        bool run = false;
        codegen::opt_level opt = codegen::opt_level::none;
        std::vector<std::string> run_args;
        std::vector<std::string> importdirs;
        bool help = false;
    } cmdopts;

    std::string const debug_compiler_str = "--debug-compiler";
    std::string const disable_color_str = "--disable-color";
    std::string const run_str = "--run";
    std::string const debug_str = "--debug";
    std::string const release_str = "--release";
    std::string const help_str = "--help";

    for (; *arg; ++arg) {
        if (boost::algorithm::starts_with(*arg, "--runtimedir=")) {
            cmdopts.libdirs += get_substitution_option(*arg, "--runtimedir=");
        } else if (boost::algorithm::ends_with(*arg, ".dcs")) {
            cmdopts.source_files.emplace_back(*arg);
        } else if (*arg == debug_compiler_str) {
            cmdopts.debug_compiler = true;
        } else if (*arg == disable_color_str) {
            cmdopts.enable_color = false;
        } else if (*arg == run_str) {
            cmdopts.run = true;
            for (; *arg; ++arg) {
                cmdopts.run_args.emplace_back(*arg);
            }
            return cmdopts;
        } else if (*arg == debug_str) {
            cmdopts.opt = codegen::opt_level::debug;
        } else if (*arg == release_str) {
            cmdopts.opt = codegen::opt_level::release;
        } else if (boost::algorithm::starts_with(*arg, "--libdir=")) {
            cmdopts.importdirs += get_substitution_option(*arg, "--libdir=");
        } else if (*arg == help_str) {
            cmdopts.help = true;
        } else {
            cmdopts.rest_args.emplace_back(*arg);
        }
    }

    return cmdopts;
}

std::string get_tmpdir()
{
    auto *const tmp = std::getenv("TMPDIR");

    if (tmp) {
        std::string ret = tmp;
        if (!ret.empty()) {
            return ret.back() == '/' ? ret : std::move(ret) + '/';
        }
    }

    return "/tmp/";
}

} // namespace cmdline
} // namespace dachs

int main(int const, char const* const argv[])
{
    (U'ω') /* Hello, Dachs! */;

    auto const& add_signal_action
        = [&](auto const signal, auto &new_action)
        {
            sigemptyset(&new_action.sa_mask);
            sigaddset(&new_action.sa_mask, signal);
            new_action.sa_handler = dachs::cmdline::signal_handler;

            return sigaction(signal, &new_action, nullptr) != -1;
        };

    struct sigaction sigsegv_action, sigabrt_action;
    if (!add_signal_action(SIGSEGV, sigsegv_action) ||
        !add_signal_action(SIGABRT, sigabrt_action)) {
        std::cerr << "Failed to set signal handler." << std::endl;
        return 4;
    }

    auto const show_usage =
        [argv]()
        {
            std::cerr << "Overview: Dachs compiler\n\n"
                      << "Usage: " << argv[0] << " [--dump-ast|--dump-sym-table|--emit-llvm|--output-obj|--check-syntax] [--debug-compiler] [--debug|--release] [--libdir={path}] [--runtimedir={path}] [--disable-color] {file} [--run [args...]]\n" <<
R"(
OPTIONS
  --dump-ast           Output AST to STDOUT
  --dump-sym-table     Output symbol table to STDOUT
  --emit-llvm          Print LLVM IR to STDOUT
  --output-obj         Generate object file instead of executable
  --check-syntax       Check syntax and output parse error if exists
  --debug-compiler     Output debug information to STDERR
  --debug              Do not optimize (equivalent to -O0)
  --release            Do aggressive optimization (equivalent to -O3)
  --libdir={path}      Add import path
  --runtimedir={path}  Specify path of runtime directory
  --disable-color      Disable colorful output
  --run [ARGS]...      Instantly run the program instead of generating executable
                       All arguments after --run are treated as runtime options
  --help               Show this help

URL
  https://github.com/rhysd/Dachs
)";
        };

    // TODO: Use Boost.ProgramOptions

    auto const cmdopts = dachs::cmdline::parse_command_options(&argv[1]);

    if (cmdopts.help) {
        show_usage();
        return 0;
    }

    if (cmdopts.source_files.empty()) {
        std::cerr << "No input file: Source file must end with '.dcs'.\n";
        return 2;
    }

    dachs::compiler compiler{cmdopts.enable_color, cmdopts.debug_compiler, cmdopts.opt};

    switch (cmdopts.rest_args.size()) {

    case 1: {
        std::string const opt = cmdopts.rest_args[0];

        if (opt == "--dump-ast") {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.dump_asts(
                        std::cout,
                        cmdopts.source_files,
                        cmdopts.importdirs
                    );
                }
            );
        } else if (opt == "--dump-sym-table") {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.dump_scope_trees(
                        std::cout,
                        cmdopts.source_files,
                        cmdopts.importdirs
                    );
                }
            );
        } else if (opt == "--emit-llvm") {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.dump_llvm_irs(
                        std::cout,
                        cmdopts.source_files
                        , cmdopts.importdirs
                    );
                }
            );
        } else if (opt == "--output-obj") {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.compile_to_objects(
                        cmdopts.source_files,
                        cmdopts.importdirs
                    );
                }
            );
        } else if (opt == "--check-syntax") {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.check_syntax(
                        cmdopts.source_files,
                        cmdopts.importdirs
                    );
                }
            );
        }
    }
    break;

    case 0:
        if (cmdopts.run && cmdopts.source_files.size() > 0) {

            // Note:
            // When JIT compiler with LLVM JIT support will be implemented,
            // '--run' option should use it.
            return dachs::cmdline::do_compiler_action(
                    [&]
                    {
                        auto tmpdir = dachs::cmdline::get_tmpdir();
                        auto const executable = compiler.compile(
                                cmdopts.source_files,
                                cmdopts.libdirs,
                                cmdopts.importdirs,
                                std::move(tmpdir)
                            );

                        assert(!executable.empty() && executable.front() == '/');

                        std::string const args = boost::algorithm::join(
                                cmdopts.run_args,
                                " "
                            );

                        std::system(
                                (
                                    executable + " " + args
                                ).c_str()
                            );
                        std::remove(executable.c_str());
                    }
                );

        } else if (cmdopts.source_files.size() > 0) {
            return dachs::cmdline::do_compiler_action(
                [&]
                {
                    compiler.compile(
                        cmdopts.source_files,
                        cmdopts.libdirs,
                        cmdopts.importdirs
                    );
                }
            );
        }
    break;

    default:
    break;
    }

    show_usage();
    return 1;
}

