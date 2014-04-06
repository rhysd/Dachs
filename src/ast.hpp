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

enum class qualifier {
    maybe,
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
struct var_ref;
struct parameter;
struct function_call;
struct object_construct;
struct primary_expr;
struct index_access;
struct member_access;
struct postfix_expr;
struct unary_expr;
struct primary_type;
struct tuple_type;
struct array_type;
struct compound_type;
struct qualified_type;
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
struct if_expr;
struct compound_expr;
struct assignment_stmt;
struct variable_decl;
struct initialize_stmt;
struct if_stmt;
struct case_stmt;
struct return_stmt;
struct switch_stmt;
struct for_stmt;
struct while_stmt;
struct postfix_if_stmt;
struct compound_stmt;
struct function_definition;
struct procedure_definition;
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
DACHS_DEFINE_NODE_PTR(var_ref);
DACHS_DEFINE_NODE_PTR(parameter);
DACHS_DEFINE_NODE_PTR(function_call);
DACHS_DEFINE_NODE_PTR(object_construct);
DACHS_DEFINE_NODE_PTR(primary_expr);
DACHS_DEFINE_NODE_PTR(index_access);
DACHS_DEFINE_NODE_PTR(member_access);
DACHS_DEFINE_NODE_PTR(postfix_expr);
DACHS_DEFINE_NODE_PTR(unary_expr);
DACHS_DEFINE_NODE_PTR(primary_type);
DACHS_DEFINE_NODE_PTR(tuple_type);
DACHS_DEFINE_NODE_PTR(array_type);
DACHS_DEFINE_NODE_PTR(compound_type);
DACHS_DEFINE_NODE_PTR(qualified_type);
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
DACHS_DEFINE_NODE_PTR(if_expr);
DACHS_DEFINE_NODE_PTR(compound_expr);
DACHS_DEFINE_NODE_PTR(assignment_stmt);
DACHS_DEFINE_NODE_PTR(variable_decl);
DACHS_DEFINE_NODE_PTR(initialize_stmt);
DACHS_DEFINE_NODE_PTR(if_stmt);
DACHS_DEFINE_NODE_PTR(case_stmt);
DACHS_DEFINE_NODE_PTR(switch_stmt);
DACHS_DEFINE_NODE_PTR(return_stmt);
DACHS_DEFINE_NODE_PTR(for_stmt);
DACHS_DEFINE_NODE_PTR(while_stmt);
DACHS_DEFINE_NODE_PTR(postfix_if_stmt);
DACHS_DEFINE_NODE_PTR(compound_stmt);
DACHS_DEFINE_NODE_PTR(function_definition);
DACHS_DEFINE_NODE_PTR(procedure_definition);
DACHS_DEFINE_NODE_PTR(program);
#undef DACHS_DEFINE_NODE_PTR

} // namespace node

namespace alias {

// handy aliases
using statement_block = std::vector<node::compound_stmt>;

} // namespace alias

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

struct expression : public base {
    // Type type;
    virtual ~expression()
    {}
};

struct statement : public base {
    virtual ~statement()
    {}
};

struct character_literal : public expression {
    char value;

    explicit character_literal(char const c)
        : expression(), value(c)
    {}

    std::string to_string() const override
    {
        return "CHAR_LITERAL: "
            + (
                value == '\f' ? "'\\f'" :
                value == '\b' ? "'\\b'" :
                value == '\n' ? "'\\n'" :
                value == '\r' ? "'\\r'" :
                                std::string{'\'', value, '\''}
            );
    }
};

struct float_literal : public expression {
    double value;

    explicit float_literal(double const d)
        : expression(), value(d)
    {}

    std::string to_string() const override
    {
        return "FLOAT_LITERAL: " + std::to_string(value);
    }
};

struct boolean_literal : public expression {
    bool value;

    explicit boolean_literal(bool const b)
        : expression(), value(b)
    {}

    std::string to_string() const override
    {
        return std::string{"BOOL_LITERAL: "} + (value ? "true" : "false");
    }
};

struct string_literal : public expression {
    std::string value;

    explicit string_literal(std::string const& s)
        : expression(), value(s)
    {}

    std::string to_string() const override;
};

struct integer_literal : public expression {
    boost::variant< int
                  , unsigned int
    > value;

    template<class T>
    explicit integer_literal(T && v)
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const override;
};

struct array_literal : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit array_literal(std::vector<node::compound_expr> const& elems)
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit tuple_literal(std::vector<node::compound_expr> const& elems)
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct symbol_literal : public expression {
    std::string value;

    explicit symbol_literal(std::string const& s)
        : expression(), value(s)
    {}

    std::string to_string() const override
    {
        return "SYMBOL_LITERAL: " + value;
    }
};

struct literal : public expression {
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
        : expression(), value(std::forward<T>(v))
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

// This node will have kind of variable (global, member and local variables)
struct var_ref : public expression {
    node::identifier name;
    // KIND_TYPE kind;

    explicit var_ref(node::identifier const& n)
        : expression(), name(n)
    {}

    std::string to_string() const override
    {
        return "VAR_REFERENCE: ";
    }
};

struct parameter : public base {
    bool is_var;
    node::identifier name;
    boost::optional<node::qualified_type> type;

    parameter(bool const v, node::identifier const& n, boost::optional<node::qualified_type> const& t)
        : base(), is_var(v), name(n), type(t)
    {}

    std::string to_string() const override
    {
        return std::string{"PARAMETER: "} + (is_var ? "mutable" : "immutable");
    }
};

struct function_call : public expression {
    std::vector<node::compound_expr> args;

    function_call(std::vector<node::compound_expr> const& args)
        : expression(), args(args)
    {}

    std::string to_string() const override
    {
        return "FUNCTION_CALL";
    }
};

struct object_construct : public expression {
    node::qualified_type type;
    std::vector<node::compound_expr> args;

    object_construct(node::qualified_type const& t
                    , std::vector<node::compound_expr> const& args)
        : expression(), type(t), args(args)
    {}

    std::string to_string() const override
    {
        return "OBJECT_CONSTRUCT";
    }
};

struct primary_expr : public expression {
    // Note: add lambda compound_expr
    using value_type =
        boost::variant< node::object_construct
                      , node::var_ref
                      , node::literal
                      , node::compound_expr
                      >;
    value_type value;

    template<class T>
    explicit primary_expr(T && v)
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "PRIMARY_EXPR";
    }
};

struct index_access : public expression {
    node::compound_expr index_expr;

    index_access(node::compound_expr const& idx_expr)
        : expression(), index_expr(idx_expr)
    {}

    std::string to_string() const override
    {
        // return index_access ? "INDEX_ACCESS" : "INDEX_ACCESS : (no index)";
        return "INDEX_ACCESS : (no index)";
    }
};

struct member_access : public expression {
    node::identifier member_name;
    explicit member_access(node::identifier const& member_name)
        : expression(), member_name(member_name)
    {}

    std::string to_string() const override
    {
        return "MEMBER_ACCESS";
    }
};

struct postfix_expr : public expression {
    // Note: add do-end block
    node::primary_expr prefix;
    using postfix_type =
        boost::variant< node::member_access
                      , node::index_access
                      , node::function_call >;
    std::vector<postfix_type> postfixes;

    postfix_expr(node::primary_expr const& pe, std::vector<postfix_type> const& ps)
        : expression(), prefix(pe), postfixes(ps)
    {}

    std::string to_string() const override
    {
        return "POSTFIX_EXPR";
    }
};

struct unary_expr : public expression {
    std::vector<unary_operator> values;
    node::postfix_expr expr;
    unary_expr(std::vector<unary_operator> const& ops, node::postfix_expr const& expr)
        : expression(), values(ops), expr(expr)
    {}

    std::string to_string() const override;
};

struct primary_type : public base {
    using value_type =
        boost::variant< node::identifier
                      , node::qualified_type >;
    value_type value;

    template<class T>
    explicit primary_type(T const& v)
        : value(v)
    {}

    std::string to_string() const override
    {
        return "PRIMARY_TYPE";
    }
};

struct array_type : public base {
    node::qualified_type elem_type;

    explicit array_type(node::qualified_type const& elem)
        : elem_type(elem)
    {}

    std::string to_string() const override
    {
        return "ARRAY_TYPE";
    }
};

struct tuple_type : public base {
    std::vector<node::qualified_type> arg_types; // Note: length of this variable should not be 1

    explicit tuple_type(std::vector<node::qualified_type> const& args)
        : arg_types(args)
    {}

    std::string to_string() const override
    {
        return "TUPLE_TYPE";
    }
};

struct compound_type : public base {
    using value_type =
        boost::variant<node::array_type, node::tuple_type, node::primary_type>;
    value_type value;

    template<class T>
    explicit compound_type(T const& v)
        : value(v)
    {}

    std::string to_string() const override
    {
        return "COMPOUND_TYPE";
    }
};

struct qualified_type : public base {
    boost::optional<qualifier> value;
    node::compound_type type;

    qualified_type(boost::optional<qualifier> const& q, node::compound_type const& t)
        : value(q), type(t)
    {}

    std::string to_string() const override
    {
        return std::string{"QUALIFIED_TYPE: "} + (value ? std::to_string(static_cast<int>(*value)) : "");
    }
};

struct cast_expr : public expression {
    std::vector<node::qualified_type> dest_types;
    node::unary_expr source_expr;

    cast_expr(std::vector<node::qualified_type> const& types, node::unary_expr const& expr)
        : expression(), dest_types(types), source_expr(expr)
    {}

    std::string to_string() const override
    {
        return "CAST_EXPR";
    }
};

struct mult_expr : public expression {
    using rhs_type
        = std::pair<mult_operator, node::cast_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::cast_expr lhs;
    rhss_type rhss;

    mult_expr(node::cast_expr const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "MULT_EXPR";
    }
};

struct additive_expr : public expression {
    using rhs_type
        = std::pair<additive_operator, node::mult_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::mult_expr lhs;
    rhss_type rhss;

    additive_expr(node::mult_expr const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "ADDITIVE_EXPR";
    }
};

struct shift_expr : public expression {
    using rhs_type
        = std::pair<shift_operator, node::additive_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::additive_expr lhs;
    rhss_type rhss;

    shift_expr(node::additive_expr const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "SHIFT_EXPR";
    }
};

struct relational_expr : public expression {
    using rhs_type
        = std::pair<relational_operator, node::shift_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::shift_expr lhs;
    rhss_type rhss;

    relational_expr(node::shift_expr const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "RELATIONAL_EXPR";
    }
};

struct equality_expr : public expression {
    using rhs_type
        = std::pair<equality_operator, node::relational_expr>;
    using rhss_type
        = std::vector<rhs_type>;

    node::relational_expr lhs;
    rhss_type rhss;

    equality_expr(node::relational_expr const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    std::string to_string() const override
    {
        return "EQUALITY_EXPR";
    }
};

struct and_expr : public expression {
    std::vector<node::equality_expr> exprs;

    explicit and_expr(node::equality_expr const& lhs, std::vector<node::equality_expr> const& rhss)
        : expression(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "AND_EXPR";
    }
};

struct xor_expr : public expression {
    std::vector<node::and_expr> exprs;

    explicit xor_expr(node::and_expr const& lhs, std::vector<node::and_expr> const& rhss)
        : expression(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "XOR_EXPR";
    }
};

struct or_expr : public expression {
    std::vector<node::xor_expr> exprs;

    explicit or_expr(node::xor_expr const& lhs, std::vector<node::xor_expr> const& rhss)
        : expression(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "OR_EXPR";
    }
};

struct logical_and_expr : public expression {
    std::vector<node::or_expr> exprs;

    explicit logical_and_expr(node::or_expr const& lhs, std::vector<node::or_expr> const& rhss)
        : expression(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "LOGICAL_AND_EXPR";
    }
};

struct logical_or_expr : public expression {
    std::vector<node::logical_and_expr> exprs;

    explicit logical_or_expr(node::logical_and_expr const& lhs, std::vector<node::logical_and_expr> const& rhss)
        : expression(), exprs({lhs})
    {
        exprs.reserve(1+rhss.size());
        exprs.insert(std::end(exprs), std::begin(rhss), std::end(rhss));
    }

    std::string to_string() const override
    {
        return "LOGICAL_OR_EXPR";
    }
};

struct if_expr : public expression {
    if_kind kind;
    node::compound_expr condition_expr, then_expr, else_expr;
    if_expr(if_kind const kind,
            node::compound_expr const& condition,
            node::compound_expr const& then,
            node::compound_expr const& else_)
        : expression(), kind(kind), condition_expr(condition), then_expr(then), else_expr(else_)
    {}

    std::string to_string() const override
    {
        return std::string{"IF_EXPR: "} + (kind == ast::if_kind::if_ ? "if" : "unless");
    }
};

struct compound_expr : public expression {
    using expr_type
        = boost::variant<node::logical_or_expr,
                         node::if_expr>;
    expr_type child_expr;
    boost::optional<node::qualified_type> maybe_type;

    template<class T>
    compound_expr(T const& e, boost::optional<node::qualified_type> const& t)
        : expression(), child_expr(e), maybe_type(t)
    {}

    std::string to_string() const override
    {
        return "COMPOUND_EXPR";
    }
};

struct variable_decl : public base {
    bool is_var;
    node::identifier name;
    boost::optional<node::qualified_type> maybe_type;

    variable_decl(bool const var,
                  node::identifier const& name,
                  boost::optional<node::qualified_type> const& type)
        : is_var(var), name(name), maybe_type(type)
    {}

    std::string to_string() const override
    {
        return std::string{"VARIABLE_DECL: "} + (is_var ? "mutable" : "immutable");
    }
};

struct initialize_stmt : public statement {
    std::vector<node::variable_decl> var_decls;
    boost::optional<std::vector<node::compound_expr>> maybe_rhs_exprs;

    initialize_stmt(std::vector<node::variable_decl> const& vars,
                  boost::optional<std::vector<node::compound_expr>> const& rhss)
        : statement(), var_decls(vars), maybe_rhs_exprs(rhss)
    {}

    std::string to_string() const override
    {
        return "INITIALIZE_STMT";
    }
};

struct assignment_stmt : public statement {
    std::vector<node::postfix_expr> assignees;
    assign_operator assign_op;
    std::vector<node::compound_expr> rhs_exprs;

    assignment_stmt(std::vector<node::postfix_expr> const& assignees,
                    assign_operator assign_op,
                    std::vector<node::compound_expr> const& rhs_exprs)
        : statement(), assignees(assignees), assign_op(assign_op), rhs_exprs(rhs_exprs)
    {}

    std::string to_string() const override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt : public statement {
    using elseif_type
        = std::pair<node::compound_expr, alias::statement_block>;

    if_kind kind;
    node::compound_expr condition;
    alias::statement_block then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<alias::statement_block> maybe_else_stmts;

    if_stmt(if_kind const kind,
            node::compound_expr const& cond,
            alias::statement_block const& then,
            std::vector<elseif_type> const& elseifs,
            boost::optional<alias::statement_block> const& maybe_else)
        : statement(), kind(kind), condition(cond), then_stmts(then), elseif_stmts_list(elseifs), maybe_else_stmts(maybe_else)
    {}

    std::string to_string() const override
    {
        return std::string{"IF_STMT: "}
            + (kind == if_kind::if_ ? "if" : "unless");
    }
};

struct return_stmt : public statement {
    std::vector<node::compound_expr> ret_exprs;

    explicit return_stmt(std::vector<node::compound_expr> const& rets)
        : statement(), ret_exprs(rets)
    {}

    std::string to_string() const override
    {
        return "RETURN_STMT";
    }
};

struct case_stmt : public statement {
    using when_type
        = std::pair<node::compound_expr, alias::statement_block>;

    std::vector<when_type> when_stmts_list;
    boost::optional<alias::statement_block> maybe_else_stmts;

    case_stmt(std::vector<when_type> const& whens, boost::optional<alias::statement_block> const& elses)
        : statement(), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const override
    {
        return "CASE_STMT";
    }
};

struct switch_stmt : public statement {
    using when_type
        = std::pair<node::compound_expr, alias::statement_block>;

    node::compound_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<alias::statement_block> maybe_else_stmts;

    switch_stmt(node::compound_expr const& target,
                std::vector<when_type> const& whens,
                boost::optional<alias::statement_block> const& elses)
        : statement(), target_expr(target), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const override
    {
        return "SWITCH_STMT";
    }
};

struct for_stmt : public statement {
    std::vector<node::parameter> iter_vars;
    node::compound_expr range_expr;
    alias::statement_block body_stmts;

    for_stmt(std::vector<node::parameter> const& iters,
             node::compound_expr const& range,
             alias::statement_block body)
        : statement(), iter_vars(iters), range_expr(range), body_stmts(body)
    {}

    std::string to_string() const override
    {
        return "FOR_STMT";
    }
};

struct while_stmt : public statement {
    node::compound_expr condition;
    alias::statement_block body_stmts;

    while_stmt(node::compound_expr const& cond,
               alias::statement_block const& body)
        : statement(), condition(cond), body_stmts(body)
    {}

    std::string to_string() const override
    {
        return "WHILE_STMT";
    }
};

struct postfix_if_stmt : public statement {
    node::compound_expr body;
    if_kind kind;
    node::compound_expr condition;

    postfix_if_stmt(node::compound_expr const& body, if_kind const kind, node::compound_expr const& cond)
        : statement(), body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const override
    {
        return std::string{"POSTFIX_IF_STMT: "}
            + (kind == if_kind::if_ ? "if" : "unless");
    }
};

struct compound_stmt : public statement {
    using value_type =
        boost::variant<
              node::if_stmt
            , node::return_stmt
            , node::case_stmt
            , node::switch_stmt
            , node::for_stmt
            , node::while_stmt
            , node::assignment_stmt
            , node::initialize_stmt
            , node::postfix_if_stmt
            , node::compound_expr
        >;
    value_type value;

    template<class T>
    explicit compound_stmt(T && v)
        : statement(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        return "COMPOUND_STMT";
    }
};

struct function_definition : public base {
    node::identifier name;
    std::vector<node::parameter> params;
    boost::optional<node::qualified_type> return_type;
    alias::statement_block body;

    function_definition(node::identifier const& n
                      , std::vector<node::parameter> const& p
                      , boost::optional<node::qualified_type> const& ret
                      , alias::statement_block const& block)
        : base(), name(n), params(p), return_type(ret), body(block)
    {}

    std::string to_string() const override
    {
        return "FUNC_DEFINITION";
    }
};

struct procedure_definition : public base {
    node::identifier name;
    std::vector<node::parameter> params;
    alias::statement_block body;

    procedure_definition(node::identifier const& n
                      , std::vector<node::parameter> const& p
                      , alias::statement_block const& block)
        : base(), name(n), params(p), body(block)
    {}

    std::string to_string() const override
    {
        return "PROC_DEFINITION";
    }
};

struct program : public base {
    using func_def_type
        = boost::variant<
            node::function_definition,
            node::procedure_definition
        >;

    std::vector<func_def_type> inu;

    explicit program(std::vector<func_def_type> const& value)
        : base(), inu(value)
    {}

    std::string to_string() const override
    {
        return "PROGRAM";
    }
};

} // namespace node_type

namespace traits {

template<class T>
struct is_node : std::is_base_of<node_type::base, T> {};

template<class T>
struct is_expression : std::is_base_of<node_type::expression, T> {};

template<class T>
struct is_statement : std::is_base_of<node_type::statement, T> {};

} // namespace traits

struct ast {
    node::program root;
};

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
