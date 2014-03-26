#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "helper/variant.hpp"

namespace dachs {
namespace syntax {
namespace ast {

enum class unary_operator {
    positive,
    negative,
    one_complement,
    logical_negate
};

namespace node_type {

std::size_t generate_id();

// Forward class declarations
struct integer_literal;
struct character_literal;
struct float_literal;
struct boolean_literal;
struct string_literal;
struct array_literal;
struct tuple_literal;
struct literal;
struct identifier;
struct primary_expr;
struct index_access;
struct member_access;
struct function_call;
struct postfix_expr;
struct unary_expr;
struct program;

}

namespace node {

#define DACHS_DEFINE_NODE_PTR(n) using n = std::shared_ptr<node_type::n>
DACHS_DEFINE_NODE_PTR(integer_literal);
DACHS_DEFINE_NODE_PTR(character_literal);
DACHS_DEFINE_NODE_PTR(float_literal);
DACHS_DEFINE_NODE_PTR(boolean_literal);
DACHS_DEFINE_NODE_PTR(string_literal);
DACHS_DEFINE_NODE_PTR(array_literal);
DACHS_DEFINE_NODE_PTR(tuple_literal);
DACHS_DEFINE_NODE_PTR(literal);
DACHS_DEFINE_NODE_PTR(identifier);
DACHS_DEFINE_NODE_PTR(primary_expr);
DACHS_DEFINE_NODE_PTR(index_access);
DACHS_DEFINE_NODE_PTR(member_access);
DACHS_DEFINE_NODE_PTR(function_call);
DACHS_DEFINE_NODE_PTR(postfix_expr);
DACHS_DEFINE_NODE_PTR(unary_expr);
DACHS_DEFINE_NODE_PTR(program);
#undef DACHS_DEFINE_NODE_PTR

} // namespace node

// TODO: move below definitions to ast.cpp
namespace node_type {

struct base {
    base()
        : id(generate_id())
    {}

    virtual ~base()
    {}

    virtual std::string to_string() const = 0;

    std::size_t line = 0, col = 0, length = 0;
    std::size_t id;
};

struct character_literal : public base {
    char value;

    explicit character_literal(char const c)
        : value(c)
    {}

    std::string to_string() const override
    {
        return "CHAR_LITERAL: '" + std::to_string(value) + '\'';
    }
};

struct float_literal : public base {
    double value;

    explicit float_literal(double const d)
        : value(d)
    {}

    std::string to_string() const override
    {
        return "FLOAT_LITERAL: " + std::to_string(value);
    }
};

struct boolean_literal : public base {
    bool value;

    explicit boolean_literal(bool const b)
        : value(b)
    {}

    std::string to_string() const override
    {
        return std::string{"BOOL_LITERAL: "} + (value ? "true" : "false");
    }
};

struct string_literal : public base {
    std::string value;

    explicit string_literal(std::string const& s)
        : value(s)
    {}

    std::string to_string() const override
    {
        return std::string{"STRING_LITERAL: \""} + value + '"';
    }
};

struct integer_literal : public base {
    boost::variant< int
                  , unsigned int
    > value;

    template<class T>
    explicit integer_literal(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        try {
            if (helper::variant::has<int>(value)) {
                return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value);
            } else {
                return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value) + "u";
            }
        }
        catch(boost::bad_lexical_cast const& e) {
            return std::string{"INTEGER_LITERAL: "} + e.what();
        }
    }
};

struct array_literal : public base {
    // TODO: Not implemented
    std::string to_string() const override
    {
        return "ARRAY_LITERAL (Not implemented)";
    }
};

struct tuple_literal : public base {
    // TODO: Not implemented
    std::string to_string() const override
    {
        return "TUPLE_LITERAL (Not implemented)";
    }
};

struct literal : public base {
    using value_type =
        boost::variant< node::character_literal
                      , node::float_literal
                      , node::boolean_literal
                      , node::string_literal
                      , node::integer_literal
                      , node::array_literal
                      , node::tuple_literal >;
    value_type value;

    template<class T>
    explicit literal(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "LITERAL";
    }
};

struct identifier : public base {
    std::string value;

    explicit identifier(std::string const& s)
        : value(s)
    {}

    std::string to_string() const override
    {
        return "IDENTIFIER: " + value;
    }
};

struct primary_expr : public base {
    // Note: add lambda expression
    using value_type =
        boost::variant< node::identifier
                      , node::literal
                      // , node::expression
                      >;
    value_type value;

    template<class T>
    explicit primary_expr(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "PRIMARY_EXPR";
    }
};

// TODO: Not implemented
struct index_access : public base {
    // boost::optional<node::expression> index_expr;
    index_access()
    {}

    std::string to_string() const override
    {
        // return index_access ? "INDEX_ACCESS" : "INDEX_ACCESS : (no index)";
        return "INDEX_ACCESS : (no index)";
    }
};

struct member_access : public base {
    node::identifier member_name;
    explicit member_access(node::identifier const& member_name)
        : member_name(member_name)
    {}

    std::string to_string() const override
    {
        return "MEMBER_ACCESS";
    }
};

struct function_call : public base {
    // TODO: Not implemented
    // std::vector<node::expression> args;
    function_call()
    {}

    std::string to_string() const override
    {
        return "FUNCTION_CALL";
    }
};

struct postfix_expr : public base {
    // Note: add do-end block
    node::primary_expr prefix;
    using postfix_type =
        boost::variant< node::member_access
                      , node::index_access
                      , node::function_call >;
    std::vector<postfix_type> postfixes;

    postfix_expr(node::primary_expr const& pe, std::vector<postfix_type> const& ps)
        : base(), prefix(pe), postfixes(ps)
    {}

    std::string to_string() const override
    {
        return "POSTFIX_EXPR";
    }
};

struct unary_expr : public base {
    std::vector<unary_operator> values;
    node::postfix_expr expr;
    unary_expr(std::vector<unary_operator> const& ops, node::postfix_expr const& expr)
        : values(ops), expr(expr)
    {}

    std::string to_string() const override
    {
        return "UNARY_EXPR: " + boost::algorithm::join(
                values | boost::adaptors::transformed(
                    [](auto const op) -> std::string {
                        return op == ast::unary_operator::positive ? "+"
                             : op == ast::unary_operator::negative ? "-"
                             : op == ast::unary_operator::one_complement ? "~"
                             : op == ast::unary_operator::logical_negate ? "!"
                             : "unknown";
                    }
                ) , " ");
    }
};

struct program : public base {
    node::unary_expr value; // TEMPORARY

    program(node::unary_expr const& unary)
        : base(), value(unary)
    {}

    std::string to_string() const override
    {
        return "PROGRAM";
    }
};

} // namespace node_type

struct ast {
    node::program root;
};

} // namespace ast
} // namespace syntax
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
