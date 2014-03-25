#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>

#include "helper/variant.hpp"

namespace dachs {
namespace syntax {
namespace ast {

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
struct argument_expr_list;
struct function_call;
struct postfix_expr;
struct program;

}

namespace node {

using integer_literal = std::shared_ptr<node_type::integer_literal>;
using character_literal = std::shared_ptr<node_type::character_literal>;
using float_literal = std::shared_ptr<node_type::float_literal>;
using boolean_literal = std::shared_ptr<node_type::boolean_literal>;
using string_literal = std::shared_ptr<node_type::string_literal>;
using array_literal = std::shared_ptr<node_type::array_literal>;
using tuple_literal = std::shared_ptr<node_type::tuple_literal>;
using literal = std::shared_ptr<node_type::literal>;
using identifier = std::shared_ptr<node_type::identifier>;
using primary_expr = std::shared_ptr<node_type::primary_expr>;
using index_access = std::shared_ptr<node_type::index_access>;
using member_access = std::shared_ptr<node_type::member_access>;
using argument_expr_list = std::shared_ptr<node_type::argument_expr_list>;
using function_call = std::shared_ptr<node_type::function_call>;
using postfix_expr = std::shared_ptr<node_type::postfix_expr>;
using program = std::shared_ptr<node_type::program>;

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
        if (helper::variant::has<int>(value)) {
            return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value);
        } else {
            return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value) + "u";
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
    node::postfix_expr prefix_expr;
    // boost::optional<node::expression> index_expr;
    explicit index_access(node::postfix_expr const& prefix_expr)
        : prefix_expr(prefix_expr)
    {}

    std::string to_string() const override
    {
        // return index_access ? "INDEX_ACCESS" : "INDEX_ACCESS : (no index)";
        return "INDEX_ACCESS : (no index)";
    }
};

struct member_access : public base {
    node::postfix_expr prefix_expr;
    node::identifier member_name;
    explicit member_access( node::postfix_expr const& prefix_expr
                          , node::identifier const& member_name)
        : prefix_expr(prefix_expr), member_name(member_name)
    {}

    std::string to_string() const override
    {
        return "MEMBER_ACCESS";
    }
};

// TODO: Not implemented
struct argument_expr_list : public base {
    // std::vector<node::expression> args;
    explicit argument_expr_list( /*std::vector<node::expression> const& args*/ )
    {}

    std::string to_string() const override
    {
        return "ARG_EXPR_LIST";
    }
};

struct function_call : public base {
    node::argument_expr_list args;
    node::postfix_expr prefix_expr;
    function_call( node::argument_expr_list const& args
                 , node::postfix_expr const& prefix_expr)
        : args(args), prefix_expr(prefix_expr)
    {}

    std::string to_string() const override
    {
        return "FUNCTION_CALL";
    }
};

struct postfix_expr : public base {
    using value_type =
        boost::variant< node::primary_expr
                      , node::member_access
                      , node::index_access
                      , node::function_call
        >;
    value_type value;

    template<class T>
    explicit postfix_expr(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "POSTFIX_EXPR";
    }
};

struct program : public base {
    node::postfix_expr value; // TEMPORARY

    program(node::postfix_expr const& postfix)
        : base(), value(postfix)
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
