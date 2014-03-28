#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>

namespace dachs {
namespace syntax {
namespace ast {

enum class unary_operator {
    positive,
    negative,
    one_complement,
    logical_negate,
};

enum class mult_operator {
    mult,
    div,
    mod,
};

enum class additive_operator {
    add,
    sub,
};

enum class relational_operator {
    less_than,
    greater_than,
    less_than_equal,
    greater_than_equal,
};

enum class shift_operator {
    left,
    right,
};

enum class equality_operator {
    equal,
    not_equal,
};

enum class assign_operator {
    assign,
    mult,
    div,
    mod,
    add,
    sub,
    left_shift,
    right_shift,
    arithmetic_and,
    arithmetic_xor,
    arithmetic_or,
    logical_and,
    logical_or,
};

std::string to_string(unary_operator const o);
std::string to_string(mult_operator const o);
std::string to_string(additive_operator const o);
std::string to_string(relational_operator const o);
std::string to_string(shift_operator const o);
std::string to_string(equality_operator const o);
std::string to_string(assign_operator const o);

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
struct type_name;
struct cast_expr;
struct mult_expr;
struct additive_expr;
struct shift_expr;
struct relational_expr;
struct equality_expr;
struct and_expr;
struct xor_expr;
struct or_expr;
struct logical_and_expr;
struct logical_or_expr;
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
DACHS_DEFINE_NODE_PTR(type_name);
DACHS_DEFINE_NODE_PTR(cast_expr);
DACHS_DEFINE_NODE_PTR(mult_expr);
DACHS_DEFINE_NODE_PTR(additive_expr);
DACHS_DEFINE_NODE_PTR(shift_expr);
DACHS_DEFINE_NODE_PTR(relational_expr);
DACHS_DEFINE_NODE_PTR(equality_expr);
DACHS_DEFINE_NODE_PTR(and_expr);
DACHS_DEFINE_NODE_PTR(xor_expr);
DACHS_DEFINE_NODE_PTR(or_expr);
DACHS_DEFINE_NODE_PTR(logical_and_expr);
DACHS_DEFINE_NODE_PTR(logical_or_expr);
DACHS_DEFINE_NODE_PTR(program);
#undef DACHS_DEFINE_NODE_PTR

} // namespace node

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
        : base(), value(c)
    {}

    std::string to_string() const override
    {
        return "CHAR_LITERAL: '" + std::to_string(value) + '\'';
    }
};

struct float_literal : public base {
    double value;

    explicit float_literal(double const d)
        : base(), value(d)
    {}

    std::string to_string() const override
    {
        return "FLOAT_LITERAL: " + std::to_string(value);
    }
};

struct boolean_literal : public base {
    bool value;

    explicit boolean_literal(bool const b)
        : base(), value(b)
    {}

    std::string to_string() const override
    {
        return std::string{"BOOL_LITERAL: "} + (value ? "true" : "false");
    }
};

struct string_literal : public base {
    std::string value;

    explicit string_literal(std::string const& s)
        : base(), value(s)
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

    std::string to_string() const override;
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
        : base(), value(s)
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
        : base()
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
        : base(), member_name(member_name)
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
        : base()
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
        : base(), values(ops), expr(expr)
    {}

    std::string to_string() const override;
};

// FIXME: Temporary
struct type_name : public base {
    node::identifier name;

    explicit type_name(node::identifier const& name)
        : base(), name(name)
    {}

    std::string to_string() const override
    {
        return "TYPE_NAME";
    }
};

struct cast_expr : public base {
    std::vector<node::type_name> dest_types;
    node::unary_expr source_expr;

    cast_expr(std::vector<node::type_name> const& types, node::unary_expr const& expr)
        : base(), dest_types(types), source_expr(expr)
    {}

    std::string to_string() const override
    {
        return "CAST_EXPR";
    }
};

struct mult_expr : public base {
    using rhs_type
        = std::pair<mult_operator, node::cast_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::cast_expr lhs;
    rhss_type rhss;

    mult_expr(node::cast_expr const& lhs, rhss_type const& rhss)
        : lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "MULT_EXPR";
    }
};

struct additive_expr : public base {
    using rhs_type
        = std::pair<additive_operator, node::mult_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::mult_expr lhs;
    rhss_type rhss;

    additive_expr(node::mult_expr const& lhs, rhss_type const& rhss)
        : lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "ADDITIVE_EXPR";
    }
};

struct shift_expr : public base {
    using rhs_type
        = std::pair<shift_operator, node::additive_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::additive_expr lhs;
    rhss_type rhss;

    shift_expr(node::additive_expr const& lhs, rhss_type const& rhss)
        : lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "SHIFT_EXPR";
    }
};

struct relational_expr : public base {
    using rhs_type
        = std::pair<relational_operator, node::shift_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::shift_expr lhs;
    rhss_type rhss;

    relational_expr(node::shift_expr const& lhs, rhss_type const& rhss)
        : lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "RELATIONAL_EXPR";
    }
};

struct equality_expr : public base {
    using rhs_type
        = std::pair<equality_operator, node::relational_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::relational_expr lhs;
    rhss_type rhss;

    equality_expr(node::relational_expr const& lhs, rhss_type const& rhss)
        : lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "EQUALITY_EXPR";
    }
};

struct and_expr : public base {
    std::vector<node::equality_expr> exprs;

    explicit and_expr(std::vector<node::equality_expr> const& lhss, node::equality_expr const& rhs)
        : base(), exprs(lhss)
    {
        exprs.push_back(rhs);
    }

    std::string to_string() const override
    {
        return "AND_EXPR";
    }
};

struct xor_expr : public base {
    std::vector<node::and_expr> exprs;

    explicit xor_expr(std::vector<node::and_expr> const& lhss, node::and_expr const& rhs)
        : base(), exprs(lhss)
    {
        exprs.push_back(rhs);
    }

    std::string to_string() const override
    {
        return "XOR_EXPR";
    }
};

struct or_expr : public base {
    std::vector<node::xor_expr> exprs;

    explicit or_expr(std::vector<node::xor_expr> const& lhss, node::xor_expr const& rhs)
        : base(), exprs(lhss)
    {
        exprs.push_back(rhs);
    }

    std::string to_string() const override
    {
        return "OR_EXPR";
    }
};

struct logical_and_expr : public base {
    std::vector<node::or_expr> exprs;

    explicit logical_and_expr(std::vector<node::or_expr> const& lhss, node::or_expr const& rhs)
        : base(), exprs(lhss)
    {
        exprs.push_back(rhs);
    }

    std::string to_string() const override
    {
        return "LOGICAL_AND_EXPR";
    }
};

struct logical_or_expr : public base {
    std::vector<node::logical_and_expr> exprs;

    explicit logical_or_expr(std::vector<node::logical_and_expr> const& lhss, node::logical_and_expr const& rhs)
        : base(), exprs(lhss)
    {
        exprs.push_back(rhs);
    }

    std::string to_string() const override
    {
        return "LOGICAL_OR_EXPR";
    }
};

struct program : public base {
    node::logical_or_expr value; // TEMPORARY

    program(node::logical_or_expr const& logical_or)
        : base(), value(logical_or)
    {}

    std::string to_string() const override
    {
        return "PROGRAM";
    }
};

} // namespace node_type

template<class T>
struct is_node : std::is_base_of<node_type::base, T> {};

struct ast {
    node::program root;
};

} // namespace ast
} // namespace syntax
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
