#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>

#include "ast.hpp"

namespace dachs {
namespace ast {
namespace symbol {

std::string to_string(unary_operator const o)
{
    switch(o) {
    case unary_operator::positive:       return "+";
    case unary_operator::negative:       return "-" ;
    case unary_operator::one_complement: return "~" ;
    case unary_operator::logical_negate: return "!" ;
    default:                             return "unknown";
    }
}

std::string to_string(mult_operator const o)
{
    switch(o) {
    case mult_operator::mult: return "*";
    case mult_operator::div:  return "/";
    case mult_operator::mod:  return "%";
    default:                  return "unknown";
    }
}

std::string to_string(additive_operator const o)
{
    switch(o) {
    case additive_operator::add: return "+";
    case additive_operator::sub: return "-";
    default:                     return "unknown";
    }
}

std::string to_string(relational_operator const o)
{
    switch(o) {
    case relational_operator::less_than:          return "<";
    case relational_operator::greater_than:       return ">";
    case relational_operator::less_than_equal:    return "<=";
    case relational_operator::greater_than_equal: return ">=";
    default:                                      return "unknown";
    }
}

std::string to_string(shift_operator const o)
{
    switch(o) {
    case shift_operator::left:  return "<<";
    case shift_operator::right: return ">>";
    default:                    return "unknown";
    }
}

std::string to_string(equality_operator const o)
{
    switch(o) {
    case equality_operator::equal:     return "==";
    case equality_operator::not_equal: return "!=";
    default:                           return "unknown";
    }
}

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
    default:                              return "unknown";
    }
}

std::string to_string(if_kind const o)
{
    switch(o) {
    case if_kind::if_: return "if";
    case if_kind::unless: return "unless";
    default:    return "unknown";
    }
}

std::string to_string(qualifier const o)
{
    switch(o) {
    case qualifier::maybe: return "?";
    default:    return "unknown";
    }
}

std::string to_string(range_kind const o)
{
    switch(o) {
    case range_kind::exclusive: return "...";
    case range_kind::inclusive: return "..";
    default:                    return "unknown";
    }
}

} // namespace symbol

namespace node_type {

std::size_t generate_id()
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
    return "UNARY_EXPR: " + boost::algorithm::join(
            values | boost::adaptors::transformed(
                [](auto const op) { return symbol::to_string(op); }
            ), " ");
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
