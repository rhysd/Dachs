#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>

#include "ast.hpp"

namespace dachs {
namespace syntax {
namespace ast {

std::string to_string(unary_operator const o)
{
    return o == unary_operator::positive ? "+" :
           o == unary_operator::negative ? "-" :
           o == unary_operator::one_complement ? "~" :
           o == unary_operator::logical_negate ? "!" :
           "unknown";
}

std::string to_string(mult_operator const o)
{
    return o == mult_operator::mult ? "*" :
           o == mult_operator::div ? "/" :
           o == mult_operator::mod ? "%" :
           "unknown";
}

std::string to_string(additive_operator const o)
{
    return o == additive_operator::add ? "+" :
           o == additive_operator::sub ? "-" :
           "unknown";
}

std::string to_string(relational_operator const o)
{
    return o == relational_operator::less_than ? "<" :
           o == relational_operator::greater_than ? ">" :
           o == relational_operator::less_than_equal ? "<=" :
           o == relational_operator::greater_than_equal ? ">=" :
           "unknown";
}

std::string to_string(shift_operator const o)
{
    return o == shift_operator::left ? "<<" :
           o == shift_operator::right ? ">>" :
           "unknown";
}

std::string to_string(equality_operator const o)
{
    return o == equality_operator::equal ? "==" :
           o == equality_operator::not_equal ? "!=" :
           "unknown";
}

std::string to_string(assign_operator const o)
{
    return o == assign_operator::assign ? "=" :
           o == assign_operator::mult ? "*=" :
           o == assign_operator::div ? "/=" :
           o == assign_operator::mod ? "%=" :
           o == assign_operator::add ? "+=" :
           o == assign_operator::sub ? "-=" :
           o == assign_operator::left_shift ? ">>=" :
           o == assign_operator::right_shift ? "<<=" :
           o == assign_operator::arithmetic_and ? "&=" :
           o == assign_operator::arithmetic_xor ? "^=" :
           o == assign_operator::arithmetic_or ? "|=" :
           o == assign_operator::logical_and ? "&&=" :
           o == assign_operator::logical_or ? "||=" :
           "unknown";
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
                [](auto const op) { return syntax::ast::to_string(op); }
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
} // namespace syntax
} // namespace dachs
