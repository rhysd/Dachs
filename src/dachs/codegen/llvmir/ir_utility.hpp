#if !defined DACHS_CODEGEN_LLVMIR_IR_UTILITY_HPP_INCLUDED
#define      DACHS_CODEGEN_LLVMIR_IR_UTILITY_HPP_INCLUDED

#include <llvm/IR/Value.h>

#include <boost/format.hpp>
#include <string>
#include <memory>

#include "dachs/ast/ast.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace codegen {
namespace llvmir {

template<class String>
[[noreturn]]
inline void error(ast::location_type const& l, String const& msg)
{
    // TODO:
    // Dump builder's debug information and context's information
    throw code_generation_error{
            "LLVM IR generator",
            (
                boost::format(" in line:%1%, col:%2%\n  %3%\n")
                % std::get<ast::location::location_index::line>(l)
                % std::get<ast::location::location_index::col>(l)
                % msg
            ).str()
        };
}

template<class String>
[[noreturn]]
inline void error(String const& msg)
{
    throw code_generation_error{"LLVM IR generator", msg};
}

template<class Node, class String>
[[noreturn]]
inline void error(std::shared_ptr<Node> const& n, String const& msg)
{
    throw code_generation_error{
            "LLVM IR generator",
            (boost::format(" in line:%1%, col:%2%\n  %3%\n") % n->line % n->col % msg).str()
        };
}

template<class Node>
[[noreturn]]
inline void error(std::shared_ptr<Node> const& n, boost::format const& msg)
{
    error(n, msg.str());
}

[[noreturn]]
inline void error(ast::location_type const& l, boost::format const& msg)
{
    error(l, msg.str());
}

template<class T, class String>
T check(T const v, String const& msg)
{
    if (!v) {
        throw code_generation_error{"LLVM IR generator", msg};
    }
    return v;
}

template<class Node, class T, class String>
inline T check(Node const& n, T const v, String const& feature_name)
{
    if (!v) {
        error(n, std::string{"Failed to emit "} + feature_name);
    }
    return v;
}

template<class Node, class T>
inline T check(Node const& n, T const v, boost::format const& fmt)
{
    return check(n, v, fmt.str());
}

template<class Node, class String, class Value>
inline void check_all(Node const& n, String const& feature, Value const v)
{
    if (!v) {
        error(n, std::string{"Failed to emit "} + feature);
    }
}

template<class Node, class String, class Value, class... Values>
inline void check_all(Node const& n, String const& feature, Value const v, Values const... vs)
{
    if (!v) {
        error(n, std::string{"Failed to emit "} + feature);
    }
    check_all(n, feature, vs...);
}

} // namespace llvmir
} // namespace codegen
} // namespace dachs

#endif    // DACHS_CODEGEN_LLVMIR_IR_UTILITY_HPP_INCLUDED
