#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include <boost/optional.hpp>

#include <llvm/Support/raw_ostream.h>

#include "dachs/compiler.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/stringize_ast.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/semantics/stringize_scope_tree.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/codegen/llvmir/executable_generator.hpp"
#include "dachs/codegen/llvmir/context.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {

compiler::compiler(bool const colorful, bool const d, codegen::opt_level const o)
    : debug(d), opt(o)
{
    helper::colorizer::enabled = colorful;
}

std::string compiler::read(std::string const& file) const
{
    auto const maybe_code = dachs::helper::read_file<std::string>(file);
    if (!maybe_code) {
        throw std::runtime_error{"File cannot be opened: " + file};
    }
    return *maybe_code;
}

std::string compiler::compile(compiler::files_type const& files, std::vector<std::string> const& libdirs, files_type const& importdirs, std::string parent) const
{
    std::vector<llvm::Module *> modules;
    codegen::llvmir::context context;

    for (auto const& f : files) {
        auto const code = read(f);
        if (debug) {
            std::cerr << "file: " << f << '\n';
        }

        auto ast = parser.parse(code, f);
        syntax::import(ast, importdirs, parser, f);
        if (debug) {
            std::cerr << ast::stringize_ast(ast) << "\n\n";
        }

        auto ctx = semantics::analyze_semantics(ast);
        if (debug) {
            std::cerr << "=========Scope Tree=========\n\n"
                      <<  scope::stringize_scope_tree(ctx.scopes) << "\n\n";

        }

        auto &module = codegen::llvmir::emit_llvm_ir(ast, ctx, context);
        if (debug) {
            std::cerr << "=========LLVM IR=========\n\n";
            module.dump();
        }

        modules.push_back(&module);
    }

    return codegen::llvmir::generate_executable(modules, libdirs, context, opt, std::move(parent));
}

std::vector<std::string> compiler::compile_to_objects(compiler::files_type const& files, files_type const& importdirs, std::string parent) const
{
    std::vector<llvm::Module *> modules;
    codegen::llvmir::context context;

    for (auto const& f : files) {
        auto const code = read(f);
        auto ast = parser.parse(code, f);
        import(ast, importdirs, parser, f);
        auto semantics = semantics::analyze_semantics(ast);
        auto &module = codegen::llvmir::emit_llvm_ir(ast, semantics, context);
        if (debug) {
            std::cerr << "file: " << f << '\n'
                      << ast::stringize_ast(ast)
                            + "\n\n=========Scope Tree=========\n\n"
                            + scope::stringize_scope_tree(semantics.scopes)
                    << "\n\n=========LLVM IR=========\n\n";
            module.dump();
        }
        modules.push_back(&module);
    }

    return codegen::llvmir::generate_objects(modules, context, opt, parent);
}

std::string compiler::report_ast(std::string const& file, std::string const& code, files_type const& importdirs) const
{
    return ast::stringize_ast(
            import(
                parser.parse(code, file),
                importdirs,
                parser,
                file
            )
        );
}

std::string compiler::report_scope_tree(std::string const& file, std::string const& code, files_type const& importdirs) const
{
    auto ast = parser.parse(code, file);
    import(ast, importdirs, parser, file);
    auto ctx = semantics::analyze_semantics(ast);
    return scope::stringize_scope_tree(ctx.scopes);
}

std::string compiler::report_llvm_ir(std::string const& file, std::string const& code, files_type const& importdirs) const
{
    auto ast = parser.parse(code, file);
    import(ast, importdirs, parser, file);
    auto ctx = semantics::analyze_semantics(ast);

    std::string result;
    llvm::raw_string_ostream raw_os{result};

    codegen::llvmir::context context;
    codegen::llvmir::emit_llvm_ir(ast, ctx, context).print(raw_os, nullptr);
    return result;
}

bool compiler::check_syntax(std::vector<std::string> const& files, files_type const& importdirs) const
{
    boost::optional<parse_error> failed;

    for (auto const& f : files) {
        try {
            import(
                parser.parse(read(f), f),
                importdirs,
                parser,
                f
            );
        }
        catch (parse_error const& e) {
            failed = e;
        }
    }

    if (failed) {
        throw *failed;
    }

    return static_cast<bool>(!failed);
}

void compiler::dump_asts(std::ostream &out, compiler::files_type const& files, files_type const& importdirs) const
{
   for (auto const& f : files) {
       out << "file: " << f << '\n'
           << report_ast(f, read(f), importdirs) << '\n';
   }
}

void compiler::dump_scope_trees(std::ostream &out, compiler::files_type const& files, files_type const& importdirs) const
{
   for (auto const& f : files) {
       out << "file: " << f << '\n'
           << report_scope_tree(f, read(f), importdirs) << '\n';
   }
}

void compiler::dump_llvm_irs(std::ostream &out, compiler::files_type const& files, files_type const& importdirs) const
{
   for (auto const& f : files) {
       out << report_llvm_ir(f, read(f), importdirs) << "\n\n";
   }
}

}  // namespace dachs
