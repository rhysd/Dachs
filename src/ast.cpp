#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>

#include "ast.hpp"

namespace dachs {
namespace ast {

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

namespace node_type {

std::size_t generate_id()
{
    static std::size_t current_id = 0;
    return ++current_id;
}

std::string unary_expr::to_string() const
{
    return "UNARY_EXPR: " + boost::algorithm::join(
            values | boost::adaptors::transformed(
                [](auto const op) { return ::dachs::ast::to_string(op); }
            ), " ");
}

std::string integer_literal::to_string() const
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
