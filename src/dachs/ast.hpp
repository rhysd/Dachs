#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>

#include "dachs/ast_fwd.hpp"
#include "dachs/scope_fwd.hpp"

namespace dachs {
namespace ast {

namespace node_type {

std::size_t generate_id() noexcept;

struct base {
    base() noexcept
        : id(generate_id())
    {}

    virtual ~base() noexcept
    {}

    virtual std::string to_string() const noexcept = 0;

    std::size_t line = 0, col = 0, length = 0;
    std::size_t id;
};

struct expression : public base {
    // Type type;
    virtual ~expression() noexcept
    {}
};

struct statement : public base {
    virtual ~statement() noexcept
    {}
};

struct character_literal final : public expression {
    char value;

    explicit character_literal(char const c) noexcept
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

    explicit float_literal(double const d) noexcept
        : expression(), value(d)
    {}

    std::string to_string() const noexcept override
    {
        return "FLOAT_LITERAL: " + std::to_string(value);
    }
};

struct boolean_literal final : public expression {
    bool value;

    explicit boolean_literal(bool const b) noexcept
        : expression(), value(b)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"BOOL_LITERAL: "} + (value ? "true" : "false");
    }
};

struct string_literal final : public expression {
    std::string value;

    explicit string_literal(std::string const& s) noexcept
        : expression(), value(s)
    {}

    std::string to_string() const noexcept override;
};

struct integer_literal final : public expression {
    boost::variant< int
                  , unsigned int
    > value;

    template<class T>
    explicit integer_literal(T && v) noexcept
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const noexcept override;
};

struct array_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit array_literal(std::vector<node::compound_expr> const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal final : public expression {
    std::vector<node::compound_expr> element_exprs;

    explicit tuple_literal(decltype(element_exprs) const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct symbol_literal final : public expression {
    std::string value;

    explicit symbol_literal(std::string const& s) noexcept
        : expression(), value(s)
    {}

    std::string to_string() const noexcept override
    {
        return "SYMBOL_LITERAL: " + value;
    }
};

struct dict_literal final : public expression {
    using dict_elem_type =
        std::pair<node::compound_expr, node::compound_expr>;
    using value_type =
        std::vector<dict_elem_type>;

    value_type value;

    explicit dict_literal(value_type const& m) noexcept
        : value(m)
    {}

    std::string to_string() const noexcept override
    {
        return "dict_LITERAL: size=" + std::to_string(value.size());
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
                      , node::dict_literal
                      , node::tuple_literal
                >;
    value_type value;

    template<class T>
    explicit literal(T && v) noexcept
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const noexcept override
    {
        return "LITERAL";
    }
};

struct identifier final : public base {
    std::string value;

    explicit identifier(std::string const& s) noexcept
        : base(), value(s)
    {}

    std::string to_string() const noexcept override
    {
        return "IDENTIFIER: " + value;
    }
};

// This node will have kind of variable (global, member, local variables and functions)
struct var_ref final : public expression {
    node::identifier name;
    boost::variant<dachs::symbol::weak_var_symbol, scope::weak_func_scope> symbol;

    explicit var_ref(node::identifier const& n) noexcept
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
    boost::optional<node::qualified_type> param_type;
    dachs::symbol::weak_var_symbol param_symbol;
    boost::optional<dachs::symbol::template_type_symbol> template_type_ref;

    parameter(bool const v, node::identifier const& n, decltype(param_type) const& t) noexcept
        : base(), is_var(v), name(n), param_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"PARAMETER: "} + (is_var ? "mutable" : "immutable");
    }
};

struct function_call final : public expression {
    std::vector<node::compound_expr> args;

    function_call(decltype(args) const& args) noexcept
        : expression(), args(args)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNCTION_CALL";
    }
};

struct object_construct final : public expression {
    node::qualified_type obj_type;
    std::vector<node::compound_expr> args;

    object_construct(node::qualified_type const& t,
                     decltype(args) const& args) noexcept
        : expression(), obj_type(t), args(args)
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
    explicit primary_expr(T && v) noexcept
        : expression(), value(std::forward<T>(v))
    {}

    std::string to_string() const noexcept override
    {
        return "PRIMARY_EXPR";
    }
};

struct index_access final : public expression {
    node::compound_expr index_expr;

    index_access(node::compound_expr const& idx_expr) noexcept
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

    explicit member_access(node::identifier const& member_name) noexcept
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

    postfix_expr(node::primary_expr const& pe, std::vector<postfix_type> const& ps) noexcept
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

    unary_expr(std::vector<symbol::unary_operator> const& ops, node::postfix_expr const& expr) noexcept
        : expression(), values(ops), expr(expr)
    {}

    std::string to_string() const noexcept override;
};

struct template_type final : public base {
    node::identifier template_name;
    boost::optional<std::vector<node::qualified_type>> instantiated_types;

    template_type(node::identifier const& tmpl
                , decltype(instantiated_types) const& instantiated) noexcept
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
    explicit primary_type(T const& v) noexcept
        : value(v)
    {}

    std::string to_string() const noexcept override
    {
        return "PRIMARY_TYPE";
    }
};

struct array_type final : public base {
    node::qualified_type elem_type;

    explicit array_type(node::qualified_type const& elem) noexcept
        : elem_type(elem)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_TYPE";
    }
};

struct dict_type final : public base {
    node::qualified_type key_type;
    node::qualified_type value_type;

    dict_type(node::qualified_type const& key_type,
             node::qualified_type const& value_type) noexcept
        : key_type(key_type), value_type(value_type)
    {}

    std::string to_string() const noexcept override
    {
        return "dict_TYPE";
    }
};

struct tuple_type final : public base {
    std::vector<node::qualified_type> arg_types; // Note: length of this variable should not be 1

    explicit tuple_type(std::vector<node::qualified_type> const& args) noexcept
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
            , node::qualified_type const& ret_t) noexcept
        : base(), arg_types(arg_t), ret_type(ret_t)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_TYPE";
    }
};

struct proc_type final : public base {
    std::vector<node::qualified_type> arg_types;

    explicit proc_type(decltype(arg_types) const& arg_t) noexcept
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
                     , node::dict_type
                     , node::func_type
                     , node::proc_type
                     , node::primary_type>;
    value_type value;

    template<class T>
    explicit compound_type(T const& v) noexcept
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
                   node::compound_type const& t) noexcept
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
              node::unary_expr const& expr) noexcept
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

    multi_binary_expr(FactorType const& lhs, rhss_type const& rhss) noexcept
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

    binary_expr(FactorType const& lhs, decltype(rhss) const& rhss) noexcept
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
            node::compound_expr const& else_) noexcept
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
               decltype(maybe_rhs) const& maybe_rhs) noexcept
        : lhs(lhs), maybe_rhs(maybe_rhs)
    {}

    bool has_range() const noexcept
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
    compound_expr(T const& e, decltype(maybe_type) const& t) noexcept
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
    dachs::symbol::weak_var_symbol symbol;

    variable_decl(bool const var,
                  node::identifier const& name,
                  decltype(maybe_type) const& type) noexcept
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
                    decltype(maybe_rhs_exprs) const& rhss) noexcept
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
                    decltype(rhs_exprs) const& rhs_exprs) noexcept
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
            decltype(maybe_else_stmts) const& maybe_else) noexcept
        : statement(), kind(kind), condition(cond), then_stmts(then), elseif_stmts_list(elseifs), maybe_else_stmts(maybe_else)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_STMT: " + symbol::to_string(kind);
    }
};

struct return_stmt final : public statement {
    std::vector<node::compound_expr> ret_exprs;

    explicit return_stmt(std::vector<node::compound_expr> const& rets) noexcept
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
              decltype(maybe_else_stmts) const& elses) noexcept
        : statement(), when_stmts_list(whens), maybe_else_stmts(elses)
    {}

    std::string to_string() const noexcept override
    {
        return "CASE_STMT";
    }
};

struct switch_stmt final : public statement {
    using when_type
        = std::pair<
            std::vector<node::compound_expr>,
            node::statement_block
        >;

    node::compound_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;

    switch_stmt(node::compound_expr const& target,
                decltype(when_stmts_list) const& whens,
                decltype(maybe_else_stmts) const& elses) noexcept
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
             node::statement_block body) noexcept
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
               node::statement_block const& body) noexcept
        : statement(), condition(cond), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "WHILE_STMT";
    }
};

struct postfix_if_stmt final : public statement {
    using body_type
        = boost::variant<
            node::assignment_stmt
          , node::return_stmt
          , node::compound_expr
        >;
    body_type body;
    symbol::if_kind kind;
    node::compound_expr condition;

    template<class T>
    postfix_if_stmt(T const& body,
                    symbol::if_kind const kind,
                    node::compound_expr const& cond) noexcept
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
    explicit compound_stmt(T && v) noexcept
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
    scope::weak_local_scope scope;

    explicit statement_block(block_type const& v) noexcept
        : value(v)
    {}

    explicit statement_block(boost::optional<block_type> const& ov) noexcept
        : value(ov ? *ov : block_type{})
    {}

    std::string to_string() const noexcept override
    {
        return "STATEMENT_BLOCK: size=" + std::to_string(value.size());
    }
};

struct function_definition final : public statement {
    node::identifier name;
    std::vector<node::parameter> params;
    boost::optional<node::qualified_type> return_type;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;
    scope::weak_func_scope scope;

    function_definition(node::identifier const& n
                      , decltype(params) const& p
                      , decltype(return_type) const& ret
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure) noexcept
        : statement(), name(n), params(p), return_type(ret), body(block), ensure_body(ensure)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_DEFINITION";
    }
};

struct procedure_definition final : public statement {
    node::identifier name;
    std::vector<node::parameter> params;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;
    scope::weak_func_scope scope;

    procedure_definition(node::identifier const& n
                      , decltype(params) const& p
                      , node::statement_block const& block
                      , decltype(ensure_body) const& ensure) noexcept
        : statement(), name(n), params(p), body(block), ensure_body(ensure)
    {}

    std::string to_string() const noexcept override
    {
        return "PROC_DEFINITION";
    }
};

struct constant_decl final : public base {
    node::identifier name;
    boost::optional<node::qualified_type> maybe_type;
    dachs::symbol::weak_var_symbol symbol;

    constant_decl(node::identifier const& name,
                  decltype(maybe_type) const& type) noexcept
        : base(), name(name), maybe_type(type)
    {}

    std::string to_string() const noexcept override
    {
        return "CONSTANT_DECL";
    }
};

struct constant_definition final : public statement {
    std::vector<node::constant_decl> const_decls;
    std::vector<node::compound_expr> initializers;

    constant_definition(decltype(const_decls) const& const_decls
                      , decltype(initializers) const& initializers) noexcept
        : statement(), const_decls(const_decls), initializers(initializers)
    {}

    std::string to_string() const noexcept override
    {
        return "CONSTANT_DEFINITION";
    }
};

struct global_definition final : public base {
    using value_type =
        boost::variant<
            node::function_definition,
            node::procedure_definition,
            node::constant_definition
        >;
    value_type value;

    template<class T>
    explicit global_definition(T const& v) noexcept
        : value(v)
    {}

    std::string to_string() const noexcept override
    {
        return "GLOBAL_DEFINITION";
    }
};

struct program final : public base {
    std::vector<node::global_definition> inu;

    explicit program(decltype(inu) const& value) noexcept
        : base(), inu(value)
    {}

    std::string to_string() const noexcept override
    {
        return "PROGRAM: num of definitions is " + std::to_string(inu.size());
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
