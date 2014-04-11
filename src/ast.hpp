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
namespace symbol {

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

enum class range_kind {
    exclusive,
    inclusive,
};

std::string to_string(unary_operator const o);
std::string to_string(mult_operator const o);
std::string to_string(additive_operator const o);
std::string to_string(relational_operator const o);
std::string to_string(shift_operator const o);
std::string to_string(equality_operator const o);
std::string to_string(assign_operator const o);
std::string to_string(if_kind const o);
std::string to_string(qualifier const o);
std::string to_string(range_kind const o);

} // namespace symbol

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
struct map_literal;
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
struct template_type;
struct primary_type;
struct tuple_type;
struct func_type;
struct proc_type;
struct array_type;
struct map_type;
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
struct range_expr;
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
struct statement_block;
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
DACHS_DEFINE_NODE_PTR(map_literal);
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
DACHS_DEFINE_NODE_PTR(template_type);
DACHS_DEFINE_NODE_PTR(primary_type);
DACHS_DEFINE_NODE_PTR(tuple_type);
DACHS_DEFINE_NODE_PTR(func_type);
DACHS_DEFINE_NODE_PTR(proc_type);
DACHS_DEFINE_NODE_PTR(array_type);
DACHS_DEFINE_NODE_PTR(map_type);
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
DACHS_DEFINE_NODE_PTR(range_expr);
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
DACHS_DEFINE_NODE_PTR(statement_block);
DACHS_DEFINE_NODE_PTR(function_definition);
DACHS_DEFINE_NODE_PTR(procedure_definition);
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

    virtual std::string to_string() const noexcept = 0;

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

struct character_literal final : public expression {
    char value;

    explicit character_literal(char const c)
        : expression(), value(c)
    {}

    std::string to_string() const noexcept override
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

struct float_literal final : public expression {
    double value;

    explicit float_literal(double const d)
        : expression(), value(d)
    {}

    std::string to_string() const noexcept override
    {
        return "FLOAT_LITERAL: " + std::to_string(value);
    }
};

struct boolean_literal final : public expression {
    bool value;

    explicit boolean_literal(bool const b)
        : expression(), value(b)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"BOOL_LITERAL: "} + (value ? "true" : "false");
    }
};

struct string_literal final : public expression {
    std::string value;

    explicit string_literal(std::string const& s)
        : expression(), value(s)
    {}

    std::string to_string() const noexcept override;
};

struct integer_literal final : public expression {
    boost::variant< int
                  , unsigned int
    > value;

    template<class T>
    explicit integer_literal(T && v)
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const noexcept override;
};

struct array_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit array_literal(std::vector<node::compound_expr> const& elems)
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit tuple_literal(decltype(element_exprs) const& elems)
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct symbol_literal final : public expression {
    std::string value;

    explicit symbol_literal(std::string const& s)
        : expression(), value(s)
    {}

    std::string to_string() const noexcept override
    {
        return "SYMBOL_LITERAL: " + value;
    }
};

struct map_literal final : public expression {
    using map_elem_type =
        std::pair<node::compound_expr, node::compound_expr>;
    using value_type =
        std::vector<map_elem_type>;

    value_type value;

    explicit map_literal(value_type const& m)
        : value(m)
    {}

    std::string to_string() const noexcept override
    {
        return "MAP_LITERAL: size=" + std::to_string(value.size());
    }
};

struct literal final : public expression {
    using value_type =
        boost::variant< node::character_literal
                      , node::float_literal
                      , node::boolean_literal
                      , node::string_literal
                      , node::integer_literal
                      , node::array_literal
                      , node::symbol_literal
                      , node::map_literal
                      , node::tuple_literal
                >;
    value_type value;

    template<class T>
    explicit literal(T && v)
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const noexcept override
    {
        return "LITERAL";
    }
};

struct identifier final : public base {
    std::string value;

    explicit identifier(std::string const& s)
        : base(), value(s)
    {}

    std::string to_string() const noexcept override
    {
        return "IDENTIFIER: " + value;
    }
};

// This node will have kind of variable (global, member and local variables)
struct var_ref final : public expression {
    node::identifier name;
    // KIND_TYPE kind;

    explicit var_ref(node::identifier const& n)
        : expression(), name(n)
    {}

    std::string to_string() const noexcept override
    {
        return "VAR_REFERENCE: ";
    }
};

struct parameter final : public base {
    bool is_var;
    node::identifier name;
    boost::optional<node::qualified_type> type;

    parameter(bool const v, node::identifier const& n, decltype(type) const& t)
        : base(), is_var(v), name(n), type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"PARAMETER: "} + (is_var ? "mutable" : "immutable");
    }
};

struct function_call final : public expression {
    std::vector<node::compound_expr> args;

    function_call(decltype(args) const& args)
        : expression(), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNCTION_CALL";
    }
};

struct object_construct final : public expression {
    node::qualified_type type;
    std::vector<node::compound_expr> args;

    object_construct(node::qualified_type const& t,
                     decltype(args) const& args)
        : expression(), type(t), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "OBJECT_CONSTRUCT";
    }
};

struct primary_expr final : public expression {
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

    std::string to_string() const noexcept override
    {
        return "PRIMARY_EXPR";
    }
};

struct index_access final : public expression {
    node::compound_expr index_expr;

    index_access(node::compound_expr const& idx_expr)
        : expression(), index_expr(idx_expr)
    {}

    std::string to_string() const noexcept override
    {
        // return index_access ? "INDEX_ACCESS" : "INDEX_ACCESS : (no index)";
        return "INDEX_ACCESS : (no index)";
    }
};

struct member_access final : public expression {
    node::identifier member_name;
    explicit member_access(node::identifier const& member_name)
        : expression(), member_name(member_name)
    {}

    std::string to_string() const noexcept override
    {
        return "MEMBER_ACCESS";
    }
};

struct postfix_expr final : public expression {
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

    std::string to_string() const noexcept override
    {
        return "POSTFIX_EXPR";
    }
};

struct unary_expr final : public expression {
    std::vector<symbol::unary_operator> values;
    node::postfix_expr expr;
    unary_expr(std::vector<symbol::unary_operator> const& ops, node::postfix_expr const& expr)
        : expression(), values(ops), expr(expr)
    {}

    std::string to_string() const noexcept override;
};

struct template_type final : public base {
    node::identifier template_name;
    boost::optional<std::vector<node::qualified_type>> instantiated_types;

    template_type(node::identifier const& tmpl
                , decltype(instantiated_types) const& instantiated)
        : base(), template_name(tmpl), instantiated_types(instantiated)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"TEMPLATE_TYPE: "} + (instantiated_types ? "template" : "not template");
    }
};

struct primary_type final : public base {
    using value_type =
        boost::variant< node::template_type
                      , node::qualified_type >;
    value_type value;

    template<class T>
    explicit primary_type(T const& v)
        : value(v)
    {}

    std::string to_string() const noexcept override
    {
        return "PRIMARY_TYPE";
    }
};

struct array_type final : public base {
    node::qualified_type elem_type;

    explicit array_type(node::qualified_type const& elem)
        : elem_type(elem)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_TYPE";
    }
};

struct map_type final : public base {
    node::qualified_type key_type;
    node::qualified_type value_type;

    map_type(node::qualified_type const& key_type,
             node::qualified_type const& value_type)
        : key_type(key_type), value_type(value_type)
    {}

    std::string to_string() const noexcept override
    {
        return "MAP_TYPE";
    }
};

struct tuple_type final : public base {
    std::vector<node::qualified_type> arg_types; // Note: length of this variable should not be 1

    explicit tuple_type(std::vector<node::qualified_type> const& args)
        : arg_types(args)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_TYPE";
    }
};

struct func_type final : public base {
    std::vector<node::qualified_type> arg_types;
    node::qualified_type ret_type;

    func_type(decltype(arg_types) const& arg_t
            , node::qualified_type const& ret_t)
        : base(), arg_types(arg_t), ret_type(ret_t)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_TYPE";
    }
};

struct proc_type final : public base {
    std::vector<node::qualified_type> arg_types;

    explicit proc_type(decltype(arg_types) const& arg_t)
        : base(), arg_types(arg_t)
    {}

    std::string to_string() const noexcept override
    {
        return "PROC_TYPE";
    }
};

struct compound_type final : public base {
    using value_type =
        boost::variant<node::array_type
                     , node::tuple_type
                     , node::map_type
                     , node::func_type
                     , node::proc_type
                     , node::primary_type>;
    value_type value;

    template<class T>
    explicit compound_type(T const& v)
        : value(v)
    {}

    std::string to_string() const noexcept override
    {
        return "COMPOUND_TYPE";
    }
};

struct qualified_type final : public base {
    boost::optional<symbol::qualifier> value;
    node::compound_type type;

    qualified_type(decltype(value) const& q,
                   node::compound_type const& t)
        : value(q), type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"QUALIFIED_TYPE: "} + (value ? symbol::to_string(*value) : "not qualified");
    }
};

struct cast_expr final : public expression {
    std::vector<node::qualified_type> dest_types;
    node::unary_expr source_expr;

    cast_expr(decltype(dest_types) const& types,
              node::unary_expr const& expr)
        : expression(), dest_types(types), source_expr(expr)
    {}

    std::string to_string() const noexcept override
    {
        return "CAST_EXPR";
    }
};

template<class FactorType, class OperatorType>
struct multi_binary_expr : public expression {
    using rhs_type
        = std::pair<OperatorType, FactorType>;
    using rhss_type
        = std::vector<rhs_type>;

    FactorType lhs;
    rhss_type rhss;

    multi_binary_expr(FactorType const& lhs, rhss_type const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    virtual ~multi_binary_expr()
    {}
};

struct mult_expr final : public multi_binary_expr<node::cast_expr, symbol::mult_operator> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "MULT_EXPR";
    }
};

struct additive_expr final : public multi_binary_expr<node::mult_expr, symbol::additive_operator> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "ADDITIVE_EXPR";
    }
};

struct shift_expr final : public multi_binary_expr<node::additive_expr, symbol::shift_operator> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "SHIFT_EXPR";
    }
};

struct relational_expr final : public multi_binary_expr<node::shift_expr, symbol::relational_operator> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "RELATIONAL_EXPR";
    }
};

struct equality_expr final : public multi_binary_expr<node::relational_expr, symbol::equality_operator> {
    using multi_binary_expr::multi_binary_expr;

    std::string to_string() const noexcept override
    {
        return "EQUALITY_EXPR";
    }
};

template<class FactorType>
struct binary_expr : public expression {
    FactorType lhs;
    std::vector<FactorType> rhss;

    binary_expr(FactorType const& lhs, decltype(rhss) const& rhss)
        : expression(), lhs(lhs), rhss(rhss)
    {}

    virtual ~binary_expr()
    {}
};

struct and_expr final : public binary_expr<node::equality_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "AND_EXPR";
    }
};

struct xor_expr final : public binary_expr<node::and_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "XOR_EXPR";
    }
};

struct or_expr final : public binary_expr<node::xor_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "OR_EXPR";
    }
};

struct logical_and_expr final : public binary_expr<node::or_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "LOGICAL_AND_EXPR";
    }
};

struct logical_or_expr final : public binary_expr<node::logical_and_expr> {
    using binary_expr::binary_expr;

    std::string to_string() const noexcept override
    {
        return "LOGICAL_OR_EXPR";
    }
};

struct if_expr final : public expression {
    symbol::if_kind kind;
    node::compound_expr condition_expr, then_expr, else_expr;
    if_expr(symbol::if_kind const kind,
            node::compound_expr const& condition,
            node::compound_expr const& then,
            node::compound_expr const& else_)
        : expression(), kind(kind), condition_expr(condition), then_expr(then), else_expr(else_)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_EXPR: " + symbol::to_string(kind);
    }
};

struct range_expr final : public expression {
    using rhs_type
        = std::pair<
            symbol::range_kind,
            node::logical_or_expr
        >;
    node::logical_or_expr lhs;
    boost::optional<rhs_type> maybe_rhs;

    range_expr(node::logical_or_expr const& lhs,
               decltype(maybe_rhs) const& maybe_rhs)
        : lhs(lhs), maybe_rhs(maybe_rhs)
    {}

    bool has_range() const
    {
        return maybe_rhs;
    }

    std::string to_string() const noexcept override
    {
        return "RANGE_EXPR: " + (maybe_rhs ? symbol::to_string((*maybe_rhs).first) : "no range");
    }
};

struct compound_expr final : public expression {
    using expr_type
        = boost::variant<node::range_expr,
                         node::if_expr>;
    expr_type child_expr;
    boost::optional<node::qualified_type> maybe_type;

    template<class T>
    compound_expr(T const& e, decltype(maybe_type) const& t)
        : expression(), child_expr(e), maybe_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "COMPOUND_EXPR";
    }
};

struct variable_decl final : public base {
    bool is_var;
    node::identifier name;
    boost::optional<node::qualified_type> maybe_type;

    variable_decl(bool const var,
                  node::identifier const& name,
                  decltype(maybe_type) const& type)
        : is_var(var), name(name), maybe_type(type)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"VARIABLE_DECL: "} + (is_var ? "mutable" : "immutable");
    }
};

struct initialize_stmt final : public statement {
    std::vector<node::variable_decl> var_decls;
    boost::optional<std::vector<node::compound_expr>> maybe_rhs_exprs;

    initialize_stmt(decltype(var_decls) const& vars,
                    decltype(maybe_rhs_exprs) const& rhss)
        : statement(), var_decls(vars), maybe_rhs_exprs(rhss)
    {}

    std::string to_string() const noexcept override
    {
        return "INITIALIZE_STMT";
    }
};

struct assignment_stmt final : public statement {
    std::vector<node::postfix_expr> assignees;
    symbol::assign_operator assign_op;
    std::vector<node::compound_expr> rhs_exprs;

    assignment_stmt(decltype(assignees) const& assignees,
                    symbol::assign_operator assign_op,
                    decltype(rhs_exprs) const& rhs_exprs)
        : statement(), assignees(assignees), assign_op(assign_op), rhs_exprs(rhs_exprs)
    {}

    std::string to_string() const noexcept override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt final : public statement {
    using elseif_type
        = std::pair<node::compound_expr, node::statement_block>;

    symbol::if_kind kind;
    node::compound_expr condition;
    node::statement_block then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    if_stmt(symbol::if_kind const kind,
            node::compound_expr const& cond,
            node::statement_block const& then,
            decltype(elseif_stmts_list) const& elseifs,
            decltype(maybe_else_stmts) const& maybe_else)
        : statement(), kind(kind), condition(cond), then_stmts(then), elseif_stmts_list(elseifs), maybe_else_stmts(maybe_else)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_STMT: " + symbol::to_string(kind);
    }
};

struct return_stmt final : public statement {
    std::vector<node::compound_expr> ret_exprs;

    explicit return_stmt(std::vector<node::compound_expr> const& rets)
        : statement(), ret_exprs(rets)
    {}

    std::string to_string() const noexcept override
    {
        return "RETURN_STMT";
    }
};

struct case_stmt final : public statement {
    using when_type
        = std::pair<node::compound_expr, node::statement_block>;

    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    case_stmt(decltype(when_stmts_list) const& whens,
              decltype(maybe_else_stmts) const& elses)
        : statement(), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const noexcept override
    {
        return "CASE_STMT";
    }
};

struct switch_stmt final : public statement {
    using when_type
        = std::pair<node::compound_expr, node::statement_block>;

    node::compound_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    switch_stmt(node::compound_expr const& target,
                decltype(when_stmts_list) const& whens,
                decltype(maybe_else_stmts) const& elses)
        : statement(), target_expr(target), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const noexcept override
    {
        return "SWITCH_STMT";
    }
};

struct for_stmt final : public statement {
    std::vector<node::parameter> iter_vars;
    node::compound_expr range_expr;
    node::statement_block body_stmts;

    for_stmt(decltype(iter_vars) const& iters,
             node::compound_expr const& range,
             node::statement_block body)
        : statement(), iter_vars(iters), range_expr(range), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "FOR_STMT";
    }
};

struct while_stmt final : public statement {
    node::compound_expr condition;
    node::statement_block body_stmts;

    while_stmt(node::compound_expr const& cond,
               node::statement_block const& body)
        : statement(), condition(cond), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "WHILE_STMT";
    }
};

struct postfix_if_stmt final : public statement {
    node::compound_expr body;
    symbol::if_kind kind;
    node::compound_expr condition;

    postfix_if_stmt(node::compound_expr const& body,
                    symbol::if_kind const kind,
                    node::compound_expr const& cond)
        : statement(), body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const noexcept override
    {
        return "POSTFIX_IF_STMT: " + symbol::to_string(kind);
    }
};

struct compound_stmt final : public statement {
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

    std::string to_string() const noexcept override
    {
        return "COMPOUND_STMT";
    }
};

struct statement_block final : public base {
    using block_type
        = std::vector<node::compound_stmt>;

    block_type value;

    explicit statement_block(block_type const& v)
        : value(v)
    {}

    explicit statement_block(boost::optional<block_type> const& ov)
        : value(ov ? *ov : block_type{})
    {}

    std::string to_string() const noexcept override
    {
        return "STATEMENT_BLOCK: size=" + std::to_string(value.size());
    }
};

struct function_definition final : public base {
    node::identifier name;
    std::vector<node::parameter> params;
    boost::optional<node::qualified_type> return_type;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;

    function_definition(node::identifier const& n
                      , decltype(params) const& p
                      , decltype(return_type) const& ret
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure)
        : base(), name(n), params(p), return_type(ret), body(block), ensure_body(ensure)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_DEFINITION";
    }
};

struct procedure_definition final : public base {
    node::identifier name;
    std::vector<node::parameter> params;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;

    procedure_definition(node::identifier const& n
                      , decltype(params) const& p
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure)
        : base(), name(n), params(p), body(block), ensure_body(ensure)
    {}

    std::string to_string() const noexcept override
    {
        return "PROC_DEFINITION";
    }
};

struct program final : public base {
    using func_def_type
        = boost::variant<
            node::function_definition,
            node::procedure_definition
        >;

    std::vector<func_def_type> inu;

    explicit program(decltype(inu) const& value)
        : base(), inu(value)
    {}

    std::string to_string() const noexcept override
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
