#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/lexical_cast.hpp>

#include "ast.hpp"

namespace dachs {
namespace syntax {
namespace ast {
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
                [](auto const op) -> std::string {
                    return op == unary_operator::positive ? "+"
                            : op == unary_operator::negative ? "-"
                            : op == unary_operator::one_complement ? "~"
                            : op == unary_operator::logical_negate ? "!"
                            : "unknown";
                }
            ) , " ");
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
