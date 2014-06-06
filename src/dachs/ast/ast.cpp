#include <cassert>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace ast {
namespace symbol {

std::string to_string(assign_operator const o)
{
    switch(o) {
    case assign_operator::assign:         return "=";
    case assign_operator::mult:           return "*=";
    case assign_operator::div:            return "/=";
    case assign_operator::mod:            return "%=";
    case assign_operator::add:            return "+=";
    case assign_operator::sub:            return "-=";
    case assign_operator::left_shift:     return ">>=";
    case assign_operator::right_shift:    return "<<=";
    case assign_operator::arithmetic_and: return "&=";
    case assign_operator::arithmetic_xor: return "^=";
    case assign_operator::arithmetic_or:  return "|=";
    case assign_operator::logical_and:    return "&&=";
    case assign_operator::logical_or:     return "||=";
    default:
        DACHS_INTERNAL_COMPILATION_ERROR
        return "unknown";
    }
}

std::string to_string(if_kind const o)
{
    switch(o) {
    case if_kind::if_: return "if";
    case if_kind::unless: return "unless";
    default:
        DACHS_INTERNAL_COMPILATION_ERROR
        return "unknown";
    }
}

std::string to_string(qualifier const o)
{
    switch(o) {
    case qualifier::maybe: return "?";
    default:
        DACHS_INTERNAL_COMPILATION_ERROR
        return "unknown";
    }
}

std::string to_string(func_kind const o) 
{
    switch(o) {
    case func_kind::func: return "func";
    case func_kind::proc: return "proc";
    default:
        DACHS_INTERNAL_COMPILATION_ERROR
        return "unknown";
    }
}

} // namespace symbol

namespace node_type {

std::size_t generate_id() noexcept
{
    static std::size_t current_id = 0;
    return ++current_id;
}

std::string primary_literal::to_string() const noexcept
{
    struct primary_literal_stringizer
        : public boost::static_visitor<std::string> {

        std::string operator()(char const c) const noexcept
        {
            return " char: "
                + (
                    c == '\f' ? "'\\f'" :
                    c == '\b' ? "'\\b'" :
                    c == '\n' ? "'\\n'" :
                    c == '\r' ? "'\\r'" :
                                    std::string{'\'', c, '\''}
                );
        }

        std::string operator()(double const d) const noexcept
        {
            return " float: " + std::to_string(d);
        }

        std::string operator()(bool const b) const noexcept
        {
            return std::string{" bool: "} + (b ? "true" : "false");
        }

        std::string operator()(std::string s) const noexcept
        {
            boost::algorithm::replace_all(s, "\\", "\\\\");
            boost::algorithm::replace_all(s, "\"", "\\\"");
            boost::algorithm::replace_all(s, "\b", "\\b");
            boost::algorithm::replace_all(s, "\f", "\\f");
            boost::algorithm::replace_all(s, "\n", "\\n");
            boost::algorithm::replace_all(s, "\r", "\\r");
            return " string: \"" + s + '"';
        }

        std::string operator()(int const i) const noexcept
        {
            return " int: " + std::to_string(i);
        }

        std::string operator()(unsigned int const ui) const noexcept
        {
            return " uint: " + std::to_string(ui);
        }

    } v;

    return "PRIMARY_LITERAL: " + boost::apply_visitor(v, value);
}

} // namespace node_type
} // namespace ast
} // namespace dachs
