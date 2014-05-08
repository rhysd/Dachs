#include <cassert>

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>

#include "dachs/ast.hpp"

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
        assert(false);
        return "unknown";
    }
}

std::string to_string(if_kind const o)
{
    switch(o) {
    case if_kind::if_: return "if";
    case if_kind::unless: return "unless";
    default:
        assert(false);
        return "unknown";
    }
}

std::string to_string(qualifier const o)
{
    switch(o) {
    case qualifier::maybe: return "?";
    default:
        assert(false);
        return "unknown";
    }
}

std::string to_string(func_kind const o) 
{
    switch(o) {
    case func_kind::func: return "func";
    case func_kind::proc: return "proc";
    default:
        assert(false);
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

std::string string_literal::to_string() const noexcept
{
    std::string s = value;
    boost::algorithm::replace_all(s, "\\", "\\\\");
    boost::algorithm::replace_all(s, "\"", "\\\"");
    boost::algorithm::replace_all(s, "\b", "\\b");
    boost::algorithm::replace_all(s, "\f", "\\f");
    boost::algorithm::replace_all(s, "\n", "\\n");
    boost::algorithm::replace_all(s, "\r", "\\r");
    return "STRING_LITERAL: \"" + s + '"';
}

std::string unary_expr::to_string() const noexcept
{
    return "UNARY_EXPR: " + boost::algorithm::join(values , " ");
}

std::string integer_literal::to_string() const noexcept
{
    try {
        return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value);
    }
    catch(boost::bad_lexical_cast const& e) {
        return std::string{"INTEGER_LITERAL: "} + e.what();
    }
}

} // namespace node_type
} // namespace ast
} // namespace dachs
