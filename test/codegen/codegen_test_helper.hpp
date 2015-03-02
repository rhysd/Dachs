#if !defined DACHS_TEST_CODEGEN_TEST_HELPER_HPP_INCLUDED
#define      DACHS_TEST_CODEGEN_TEST_HELPER_HPP_INCLUDED

#include <string>

#include "dachs/ast/ast.hpp"
#include "dachs/parser/parser.hpp"
#include "dachs/parser/importer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantic_analysis.hpp"
#include "dachs/codegen/llvmir/ir_emitter.hpp"
#include "dachs/exception.hpp"

#include <boost/test/included/unit_test.hpp>

static dachs::syntax::parser p;

#define CHECK_NO_THROW_CODEGEN_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            dachs::syntax::importer i{{}, "test_file"}; \
            auto s = dachs::semantics::analyze_semantics(t, i); \
            dachs::codegen::llvmir::context c; \
            BOOST_CHECK_NO_THROW(dachs::codegen::llvmir::emit_llvm_ir(t, s, c)); \
        } while (false);

#define CHECK_THROW_CODEGEN_ERROR(...) do { \
            auto t = p.parse((__VA_ARGS__), "test_file"); \
            dachs::syntax::importer i{{}, "test_file"}; \
            auto s = dachs::semantics::analyze_semantics(t, i); \
            dachs::codegen::llvmir::context c; \
            BOOST_CHECK_THROW(dachs::codegen::llvmir::emit_llvm_ir(t, s, c), dachs::code_generation_error); \
        } while (false);

#endif    // DACHS_TEST_CODEGEN_TEST_HELPER_HPP_INCLUDED
