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

enum class if_kind {
    if_,
    unless,
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
struct symbol_literal;
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
struct expression;
struct assignment_stmt;
struct if_stmt;
struct postfix_if_stmt;
struct statement;
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
DACHS_DEFINE_NODE_PTR(symbol_literal);
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
DACHS_DEFINE_NODE_PTR(expression);
DACHS_DEFINE_NODE_PTR(assignment_stmt);
DACHS_DEFINE_NODE_PTR(if_stmt);
DACHS_DEFINE_NODE_PTR(postfix_if_stmt);
DACHS_DEFINE_NODE_PTR(statement);
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

    std::string to_string() const override;
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
        return "STRING_LITERAL: \"" + value + '"';
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
    std::vector<node::expression> element_exprs;

    explicit array_literal(std::vector<node::expression> const& elems)
        : base(), element_exprs(elems)
    {}

    std::string to_string() const override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal : public base {
    std::vector<node::expression> element_exprs;

    explicit tuple_literal(std::vector<node::expression> const& elems)
        : base(), element_exprs(elems)
    {}

    std::string to_string() const override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct symbol_literal : public base {
    std::string value;

    explicit symbol_literal(std::string const& s)
        : base(), value(s)
    {}

    std::string to_string() const override
    {
        return "SYMBOL_LITERAL: " + value;
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
                      , node::tuple_literal
                      , node::symbol_literal >;
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
                      , node::expression
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
    node::expression index_expr;

    index_access(node::expression const& idx_expr)
        : base(), index_expr(idx_expr)
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
    std::vector<node::expression> args;

    function_call(std::vector<node::expression> const& args)
        : base(), args(args)
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

    explicit and_expr(node::equality_expr const& lhs, std::vector<node::equality_expr> const& rhss)
        : base(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "AND_EXPR";
    }
};

struct xor_expr : public base {
    std::vector<node::and_expr> exprs;

    explicit xor_expr(node::and_expr const& lhs, std::vector<node::and_expr> const& rhss)
        : base(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "XOR_EXPR";
    }
};

struct or_expr : public base {
    std::vector<node::xor_expr> exprs;

    explicit or_expr(node::xor_expr const& lhs, std::vector<node::xor_expr> const& rhss)
        : base(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "OR_EXPR";
    }
};

struct logical_and_expr : public base {
    std::vector<node::or_expr> exprs;

    explicit logical_and_expr(node::or_expr const& lhs, std::vector<node::or_expr> const& rhss)
        : base(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "LOGICAL_AND_EXPR";
    }
};

struct logical_or_expr : public base {
    std::vector<node::logical_and_expr> exprs;

    explicit logical_or_expr(node::logical_and_expr const& lhs, std::vector<node::logical_and_expr> const& rhss)
        : base(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "LOGICAL_OR_EXPR";
    }
};

struct expression : public base {
    node::logical_or_expr child_expr;
    boost::optional<node::type_name> maybe_type;

    expression(node::logical_or_expr const& e, boost::optional<node::type_name> const& t)
        : child_expr(e), maybe_type(t)
    {}

    std::string to_string() const override
    {
        return "EXPRESSION";
    }
};

struct assignment_stmt : public base {
    std::vector<node::postfix_expr> assignees;
    assign_operator assign_op;
    std::vector<node::expression> rhs_exprs;

    assignment_stmt(std::vector<node::postfix_expr> const& assignees,
                    assign_operator assign_op,
                    std::vector<node::expression> const& rhs_exprs)
        : assignees(assignees), assign_op(assign_op), rhs_exprs(rhs_exprs)
    {}

    std::string to_string() const override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt : public base {
    using stmt_list_type
        = std::vector<node::statement>;
    using elseif_type
        = std::pair<node::expression, stmt_list_type>;

    if_kind kind;
    node::expression condition;
    stmt_list_type then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<stmt_list_type> maybe_else_stmts;

    if_stmt(if_kind const kind,
            node::expression const& cond,
            stmt_list_type const& then,
            std::vector<elseif_type> const& elseifs,
            boost::optional<stmt_list_type> const& maybe_else)
        : kind(kind), condition(cond), then_stmts(then), elseif_stmts_list(elseifs), maybe_else_stmts(maybe_else)
    {}

    std::string to_string() const override
    {
        return std::string{"IF_STMT: "}
            + (kind == if_kind::if_ ? "if" : "unless");
    }
};

struct postfix_if_stmt : public base {
    node::expression body;
    if_kind kind;
    node::expression condition;

    postfix_if_stmt(node::expression const& body, if_kind const kind, node::expression const& cond)
        : body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const override
    {
        return std::string{"POSTFIX_IF_STMT: "}
            + (kind == if_kind::if_ ? "if" : "unless");
    }
};

struct statement : public base {
    using value_type =
        boost::variant<
              node::if_stmt
            , node::assignment_stmt
            , node::postfix_if_stmt
            , node::expression
        >;
    value_type value;

    template<class T>
    explicit statement(T && v)
        : value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "STATEMENT";
    }
};

struct program : public base {
    node::statement value; // TEMPORARY

    program(node::statement const& stmt)
        : base(), value(stmt)
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
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
