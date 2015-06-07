#if !defined DACHS_AST_AST_HPP_INCLUDED
#define      DACHS_AST_AST_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cstddef>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include "dachs/ast/ast_fwd.hpp"
#include "dachs/semantics/scope_fwd.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/helper/variant.hpp"

// Note:
// This AST is heterogeneous (patially homogeneous).
// Homogeneous AST with boost::variant is too hard because of high compile-time cost.
//
//     boost::variant<Node1, Node2, ..., NodeX> node;
//
// I now consider about type erasure technique to deal with all heterogeneous nodes
// as the same node like below.  This requires the list of all nodes at 'FIXME:' points.
// But I can reduce to one list using preprocessor macro.  Though double dispatch technique
// resolve this all nodes enumeration, it requires virtual function implementation in all nodes.
//
//
// #include <memory>
//
// template<class T, class... Args>
// inline
// std::unique_ptr<T> make_unique(Args &&... args)
// {
//     return std::unique_ptr<T>{new T{std::forward<Args>(args)...}};
// }
//
// // Forward declaration for test nodes
// struct ast_node_1;
// struct ast_node_2;
//
// class visitor {
//
//     struct visitor_holder_base {
//
//         virtual void visit(ast_node_1 const& lit) const = 0;
//         virtual void visit(ast_node_1 &lit) = 0;
//         virtual void visit(ast_node_2 const& lit) const = 0;
//         virtual void visit(ast_node_2 &lit) = 0;
//         // ...
//         // FIXME: So many pure virtual functions
//
//         virtual ~visitor_holder_base()
//         {}
//     };
//
//     template<class Visitor>
//     struct visitor_holder : visitor_holder_base {
//         Visitor &v;
//
//         void visit(ast_node_1 const& lit) const override { v(lit); }
//         void visit(ast_node_1 &lit) override { v(lit); }
//         void visit(ast_node_2 const& lit) const override { v(lit); }
//         void visit(ast_node_2 &lit) override { v(lit); }
//         // ...
//         // FIXME: So many virtual functions
//
//         visitor_holder(Visitor &v)
//             : v(v)
//         {}
//
//         virtual ~visitor_holder()
//         {}
//     };
//
//     std::unique_ptr<visitor_holder_base> holder;
//
// public:
//
//     template<class AnyVisitor>
//     explicit visitor(AnyVisitor &v)
//         : holder(make_unique<visitor_holder<AnyVisitor>>(v))
//     {}
//
//     visitor(visitor const&) = delete;
//     visitor &operator=(visitor const&) = delete;
//
//     template<class T>
//     void visit(T const& node) const
//     {
//         holder->visit(node);
//     }
//
//     template<class T>
//     void visit(T &node)
//     {
//         holder->visit(node);
//     }
// };
//
// class node {
//
//     struct node_holder_base {
//
//         // Interfaces here
//         virtual void apply(visitor const& v) const = 0;
//         virtual void apply(visitor &v) = 0;
//
//         virtual ~node_holder_base()
//         {}
//     };
//
//     template<class Node>
//     struct node_holder : node_holder_base {
//         Node node;
//
//         node_holder(Node const& node)
//             : node(node)
//         {}
//
//         node_holder(Node && node)
//             : node(node)
//         {}
//
//         void apply(visitor const& v) const override
//         {
//             v.visit(node);
//         }
//
//         void apply(visitor &v) override
//         {
//             v.visit(node);
//         }
//
//         virtual ~node_holder()
//         {}
//     };
//
//     std::shared_ptr<node_holder_base> holder;
//
// public:
//
//     template<class AnyNode>
//     node(AnyNode const& n)
//         : holder(std::make_shared<node_holder<AnyNode>>(n))
//     {}
//
//     void apply(visitor const& v)
//     {
//         holder->apply(v);
//     }
//
//     template<class V>
//     void apply(V const& v) const
//     {
//         holder->apply(visitor{v});
//     }
//
//     template<class V>
//     void apply(V &v)
//     {
//         holder->apply(visitor{v});
//     }
// };
//
// #include <iostream>
//
// // Test nodes
// struct ast_node_1 {
//     char value;
// };
// struct ast_node_2 {
//     float value;
// };
//
// // Test visitor
// struct printer {
//     template<class T>
//     void operator()(T const& t) const
//     {
//         std::cout << t.value << std::endl;
//     }
// };
//
// int main()
// {
//     node n1 = ast_node_1{'c'};
//     node n2 = ast_node_2{3.14};
//
//     printer p;
//     n1.apply(p);
//     n2.apply(p);
//
//     return 0;
// }
//


namespace dachs {
namespace ast {

namespace detail {

inline void dump_location(location_type const& l) noexcept
{
    std::cout << "line:" << std::get<0>(l) << ", "
              << "col:" << std::get<1>(l) << ", "
              << "len:" << std::get<2>(l) << std::endl;
}

} // namespace detail

namespace node {

template<class Node>
inline location_type location_of(std::shared_ptr<Node> const& node) noexcept
{
    return node->source_location();
}

template<class... Nodes>
inline location_type location_of(boost::variant<Nodes...> const& node) noexcept
{
    return helper::variant::apply_lambda(
                [](auto const& n){ return location_of(n); }
                , node
            );
}

template<class NodeTo, class NodeFrom>
inline void set_location(std::shared_ptr<NodeTo> const& to, std::shared_ptr<NodeFrom> const& from) noexcept
{
    to->set_source_location(*from);
}

template<class Node>
inline void set_location(std::shared_ptr<Node> const& to, location_type const& from) noexcept
{
    to->set_source_location(from);
}

template<class... Nodes, class Location>
inline void set_location(boost::variant<Nodes...> const& node, Location const& from) noexcept
{
    return helper::variant::apply_lambda(
                [&from](auto const& n){ return set_location(n, from); }
                , node
            );
}

} // namespace node

namespace node_type {

using boost::algorithm::any_of;

struct expression : public base {
    type::type type;

    expression() = default;

    template<class Type>
    explicit expression(Type && t)
        : type{std::forward<Type>(t)}
    {}

    virtual ~expression() noexcept
    {}
};

struct statement : public base {
    virtual ~statement() noexcept
    {}
};

struct primary_literal final : public expression {
    boost::variant< char
                  , double
                  , bool
                  , int
                  , unsigned int
                > value;

    template<class T>
    explicit primary_literal(T && v) noexcept
        : value{std::forward<T>(v)}
    {}

    std::string to_string() const noexcept override;
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

struct array_literal final : public expression {
    std::vector<node::any_expr> element_exprs;
    scope::weak_class_scope constructed_class_scope;
    scope::weak_func_scope callee_ctor_scope;

    explicit array_literal(std::vector<node::any_expr> const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    std::string to_string() const noexcept override
    {
        return "ARRAY_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct tuple_literal final : public expression {
    std::vector<node::any_expr> element_exprs;

    explicit tuple_literal(decltype(element_exprs) const& elems) noexcept
        : expression(), element_exprs(elems)
    {}

    tuple_literal() = default;

    std::string to_string() const noexcept override
    {
        return "TUPLE_LITERAL: size is " + std::to_string(element_exprs.size());
    }
};

struct string_literal final : public expression {
    std::string value;
    scope::weak_class_scope constructed_class_scope;
    scope::weak_func_scope callee_ctor_scope;

    template<class Str>
    explicit string_literal(Str && s) noexcept
        : expression(), value(std::forward<Str>(s))
    {}

    std::string to_string() const noexcept override
    {
        return "STRING_LITERAL: " + value;
    }
};

struct dict_literal final : public expression {
    using dict_elem_type =
        std::pair<node::any_expr, node::any_expr>;
    using value_type =
        std::vector<dict_elem_type>;

    value_type value;

    explicit dict_literal(value_type const& m) noexcept
        : value(m)
    {}

    std::string to_string() const noexcept override
    {
        return "DICT_LITERAL: size=" + std::to_string(value.size());
    }
};

struct lambda_expr final : public expression {
    node::function_definition def;
    node::tuple_literal receiver;

    explicit lambda_expr(decltype(def) const& d)
        : expression()
        , def(d)
        , receiver(make<node::tuple_literal>())
    {
        receiver->set_source_location(*this);
        receiver->type = type::make<type::tuple_type>();
    }

    std::string to_string() const noexcept override
    {
        return "LAMBDA_EXPR";
    }
};

// This node will have kind of variable (global, member, local variables and functions)
struct var_ref final : public expression {
    std::string name;
    dachs::symbol::weak_var_symbol symbol;
    bool is_lhs_of_assignment = false;

    explicit var_ref(std::string const& n) noexcept
        : expression(), name(n)
    {}

    bool is_ignored_var() const noexcept
    {
        return name == "_" && symbol.expired();
    }

    bool is_instance_var() const noexcept
    {
        return !name.empty() && (name.front() == '@');
    }

    std::string to_string() const noexcept override
    {
        return "VAR_REFERENCE: " + name;
    }
};

struct parameter final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::any_type> param_type;
    dachs::symbol::weak_var_symbol param_symbol;
    type::type type;
    bool is_receiver;

    template<class T>
    parameter(
            T const& v,
            std::string const& n,
            decltype(param_type) const& t,
            bool const r = false
    ) noexcept
        : base(), is_var(v), name(n), param_type(t), is_receiver(r)
    {}

    bool is_instance_var_init() const noexcept
    {
        return !name.empty() && (name.front() == '@');
    }

    std::string to_string() const noexcept override
    {
        return "PARAMETER: " + name + " (" + (is_var ? "mutable)" : "immutable)");
    }
};

struct func_invocation final : public expression {
    node::any_expr child;
    std::vector<node::any_expr> args;
    bool is_monad_invocation = false;
    scope::weak_func_scope callee_scope;
    bool is_ufcs = false;
    bool is_begin_end = false;
    bool is_let = false;

    void set_do_block(node::function_definition const& def)
    {
        auto const lambda = make<node::lambda_expr>(def);
        lambda->set_source_location(*def);
        lambda->receiver->set_source_location(*def);
        args.push_back(std::move(lambda));
    }

    func_invocation(
            node::any_expr const& c,
            std::vector<node::any_expr> const& a,
            boost::optional<node::function_definition> const& do_block = boost::none
        ) noexcept
        : expression(), child(c), args(a), is_ufcs(false)
    {
        if (do_block) {
            set_do_block(*do_block);
        }
    }

    // Note: For UFCS
    func_invocation(
            node::any_expr const& c,
            node::any_expr const& head,
            std::vector<node::any_expr> const& tail,
            boost::optional<node::function_definition> const& do_block= boost::none
        ) noexcept
        : expression(), child(c), args({head}), is_ufcs(true)
    {
        args.insert(args.end(), tail.begin(), tail.end());
        if (do_block) {
            set_do_block(*do_block);
        }
    }

    // Note: For UFCS
    func_invocation(
            node::function_definition const& do_block,
            node::any_expr const& c,
            node::any_expr const& arg
        ) noexcept
        : expression(), child(c), args({arg}), is_ufcs(true)
    {
        set_do_block(do_block);
    }

    // Note: For deep-copying ast::node::func_invocation
    func_invocation(
            node::any_expr const& c,
            std::vector<node::any_expr> const& a,
            bool const ufcs,
            bool const begin_end,
            bool const let
        ) noexcept
        : expression(), child(c), args(a), is_ufcs(ufcs), is_begin_end(begin_end), is_let(let)
    {}

    // Note: For begin-end and let-in expression
    func_invocation(
            node::lambda_expr const& lambda,
            bool const begin_end,
            bool const let
        ) noexcept
        : expression(), child(lambda), args(), is_ufcs(false), is_begin_end(begin_end), is_let(let)
    {}

    std::string to_string() const noexcept override
    {
        return "FUNC_INVOCATION";
    }
};

struct object_construct final : public expression {
    node::any_type obj_type;
    std::vector<node::any_expr> args;
    scope::weak_class_scope constructed_class_scope;
    scope::weak_func_scope callee_ctor_scope;

    object_construct(
            node::any_type const& t,
            decltype(args) const& a = {},
            boost::optional<node::function_definition> const& do_block = boost::none
        )
        : expression(), obj_type(t), args(a)
    {
        if (do_block) {
            auto lambda = make<node::lambda_expr>(*do_block);
            lambda->set_source_location(**do_block);
            lambda->receiver->set_source_location(**do_block);
            args.push_back(std::move(lambda));
        }
    }

    template<class CS, class FS>
    object_construct(node::any_type const& t,
                     decltype(args) const& args,
                     CS const& clazz,
                     FS const& ctor ) noexcept
        : expression()
        , obj_type(t)
        , args(args)
        , constructed_class_scope(clazz)
        , callee_ctor_scope(ctor)
    {}

    // Note:
    // This ctor is used for range expression ".." and "..."
    object_construct(std::string const& op, node::any_expr lhs, node::any_expr rhs)
        : expression()
        , obj_type(make<node::primary_type>("range"))
        , args({std::move(lhs), std::move(rhs)})
    {
        args.emplace_back(make<node::primary_literal>(op == "..."));
    }

    object_construct(object_construct const&) = default;

    std::string to_string() const noexcept override
    {
        return "OBJECT_CONSTRUCT";
    }
};

struct index_access final : public expression {
    node::any_expr child, index_expr;
    scope::weak_func_scope callee_scope;
    bool is_assign = false;

    index_access(node::any_expr const& c, node::any_expr const& idx_expr, bool const assign = false) noexcept
        : expression(), child(c), index_expr(idx_expr), is_assign(assign)
    {}

    std::string to_string() const noexcept override
    {
        return "INDEX_ACCESS";
    }
};

struct ufcs_invocation final : public expression {
    node::any_expr child;
    std::string member_name;
    scope::weak_func_scope callee_scope;
    bool is_assign = false;

    struct set_location_tag {};

    ufcs_invocation(
            node::any_expr const& c,
            std::string const& n,
            bool const a = false
        ) noexcept
        : expression(), child(c), member_name(n), is_assign(a)
    {}

    ufcs_invocation(
            node::any_expr const& c,
            std::string const& member_name,
            set_location_tag
        ) noexcept
        : ufcs_invocation(c, member_name)
    {
        set_source_location(node::location_of(c));
    }

    bool is_instance_var_access() const noexcept
    {
        return callee_scope.expired();
    }

    std::string to_string() const noexcept override
    {
        return "UFCS_INVOCATION: " + member_name;
    }
};

struct unary_expr final : public expression {
    std::string op;
    node::any_expr expr;
    scope::weak_func_scope callee_scope;

    unary_expr(std::string const& op, node::any_expr const& expr) noexcept
        : expression(), op(op), expr(expr)
    {}

    std::string to_string() const noexcept override
    {
        return "UNARY_EXPR: " + op;
    }
};

struct cast_expr final : public expression {
    node::any_expr child;
    node::any_type cast_type;
    scope::weak_func_scope callee_cast_scope;
    scope::weak_func_scope casted_func_scope;

    cast_expr(node::any_expr const& c,
              node::any_type const& t) noexcept
        : expression(), child(c), cast_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "CAST_EXPR";
    }
};

struct binary_expr final : public expression {
    node::any_expr lhs, rhs;
    std::string op;
    scope::weak_func_scope callee_scope;

    binary_expr(node::any_expr const& lhs
                    , std::string const& op
                    , node::any_expr const& rhs) noexcept
        : expression(), lhs(lhs), rhs(rhs), op(op)
    {}

    std::string to_string() const noexcept override
    {
        return "BINARY_EXPR: " + op;
    }
};

struct block_expr final : public expression {
    using block_type
        = std::vector<node::compound_stmt>;

    block_type stmts;
    node::any_expr last_expr;
    scope::weak_local_scope scope;

    template<class Expr>
    block_expr(
            block_type const& s,
            Expr && e)
        : expression()
        , stmts(s)
        , last_expr(std::forward<Expr>(e))
    {}

    std::string to_string() const noexcept override
    {
        return "BLOCK_EXPR";
    }
};

struct if_expr final : public expression {
    using elseif_type
        = std::pair<node::any_expr, node::block_expr>;

    symbol::if_kind kind;
    node::any_expr condition;
    node::block_expr then_block;
    std::vector<elseif_type> elseif_block_list;
    node::block_expr else_block;

    if_expr(symbol::if_kind const kind,
            node::any_expr const& cond,
            node::block_expr const& then,
            decltype(elseif_block_list) const& elseifs,
            node::block_expr const& else_)
        : expression()
        , kind(kind)
        , condition(cond)
        , then_block(then)
        , elseif_block_list(elseifs)
        , else_block(else_)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_EXPR: " + symbol::to_string(kind);
    }
};

struct typed_expr final : public expression {
    node::any_expr child_expr;
    node::any_type specified_type;

    typed_expr(node::any_expr const& e, node::any_type const& t) noexcept
        : expression(), child_expr(e), specified_type(t)
    {}

    std::string to_string() const noexcept override
    {
        return "TYPED_EXPR";
    }
};

struct primary_type final : public base {
    std::string name;
    std::vector<node::any_type> template_params;

    primary_type(std::string const& tmpl
                , decltype(template_params) const& instantiated)
        : base(), name(tmpl), template_params(instantiated)
    {}

    primary_type(std::string const& tmpl
                , node::any_type && param)
        : base(), name(tmpl), template_params({std::move(param)})
    {}

    primary_type(std::string const& tmpl) noexcept
        : base(), name(tmpl), template_params()
    {}

    std::string to_string() const noexcept override
    {
        return "PRIMARY_TYPE: " + name;
    }
};

struct array_type final : public base {
    boost::optional<node::any_type> elem_type = boost::none;

    explicit array_type(boost::optional<node::any_type> const& elem) noexcept
        : elem_type(elem)
    {}

    explicit array_type(node::any_type const& elem) noexcept
        : elem_type(elem)
    {}

    array_type() = default;

    std::string to_string() const noexcept override
    {
        return "ARRAY_TYPE";
    }
};

struct dict_type final : public base {
    node::any_type key_type;
    node::any_type value_type;

    dict_type(node::any_type const& key_type,
             node::any_type const& value_type) noexcept
        : key_type(key_type), value_type(value_type)
    {}

    std::string to_string() const noexcept override
    {
        return "DICT_TYPE";
    }
};

struct pointer_type final : public base {
    boost::optional<node::any_type> pointee_type = boost::none;

    explicit pointer_type(boost::optional<node::any_type> const& elem) noexcept
        : pointee_type(elem)
    {}

    explicit pointer_type(node::any_type const& elem) noexcept
        : pointee_type(elem)
    {}

    pointer_type() = default;

    std::string to_string() const noexcept override
    {
        return "POINTER_TYPE";
    }
};

struct typeof_type final : public base {
    node::any_expr expr;

    template<class E>
    explicit typeof_type(E && e)
        : expr(std::forward<E>(e))
    {}

    std::string to_string() const noexcept override
    {
        return "TYPEOF_TYPE";
    }
};

struct tuple_type final : public base {
    // Note: length of this variable should not be 1
    std::vector<node::any_type> arg_types;

    tuple_type() = default;

    explicit tuple_type(std::vector<node::any_type> const& args) noexcept
        : arg_types(args)
    {}

    std::string to_string() const noexcept override
    {
        return "TUPLE_TYPE";
    }
};

struct func_type final : public base {
    std::vector<node::any_type> arg_types;
    boost::optional<node::any_type> ret_type = boost::none;
    bool parens_missing = false;

    func_type(decltype(arg_types) const& a
            , decltype(ret_type) const& r
            , bool const p = false
    ) noexcept
        : base(), arg_types(a), ret_type(r), parens_missing(p)
    {}

    explicit func_type(decltype(arg_types) const& arg_t) noexcept
        : base(), arg_types(arg_t)
    {}

    // Note: For callable types template
    func_type() noexcept
        : base(), arg_types(), parens_missing(true)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"FUNC_TYPE: "} + (ret_type ? "func" : "proc");
    }
};

struct qualified_type final : public base {
    symbol::qualifier qualifier;
    node::any_type type;

    qualified_type(symbol::qualifier const& q,
                   node::any_type const& t) noexcept
        : qualifier(q), type(t)
    {}

    std::string to_string() const noexcept override
    {
        return std::string{"any_type: "} + symbol::to_string(qualifier);
    }
};

struct variable_decl final : public base {
    bool is_var;
    std::string name;
    boost::optional<node::any_type> maybe_type;
    dachs::symbol::weak_var_symbol symbol;
    boost::optional<bool> accessibility = boost::none;
    dachs::symbol::weak_var_symbol self_symbol;

    template<class T>
    variable_decl(T const& var,
                  std::string const& name,
                  decltype(maybe_type) const& type,
                  boost::optional<bool> accessibility = boost::none
                ) noexcept
        : is_var(var)
        , name(name)
        , maybe_type(type)
        , accessibility(accessibility)
    {}

    bool is_instance_var() const noexcept
    {
        return !name.empty() && (name.front() == '@');
    }

    bool is_public() const noexcept
    {
        // Note:
        // At the moment, all non-method functions are public.
        return accessibility ? *accessibility : true;
    }

    std::string to_string() const noexcept override
    {
        return "VARIABLE_DECL: "
            + name + " ("
            + (is_var ? "mutable" : "immutable")
            + (
                accessibility ? (
                    *accessibility ?
                        ", public"
                      : ", private"
                ) : ""
            ) + ')';
    }
};

struct initialize_stmt final : public statement {
    std::vector<node::variable_decl> var_decls;
    boost::optional<std::vector<node::any_expr>> maybe_rhs_exprs;

    initialize_stmt(decltype(var_decls) const& vars,
                    decltype(maybe_rhs_exprs) const& rhss = boost::none) noexcept
        : statement(), var_decls(vars), maybe_rhs_exprs(rhss)
    {}

    initialize_stmt(node::variable_decl const& lhs, node::any_expr const& rhs)
        : statement(), var_decls({lhs}), maybe_rhs_exprs(std::vector<node::any_expr>{rhs})
    {}

    std::string to_string() const noexcept override
    {
        return "INITIALIZE_STMT";
    }
};

struct assignment_stmt final : public statement {
    std::vector<node::any_expr> assignees;
    std::string op;
    std::vector<node::any_expr> rhs_exprs;
    std::vector<scope::weak_func_scope> callee_scopes;
    bool rhs_tuple_expansion = false;

    assignment_stmt(decltype(assignees) const& assignees,
                    std::string const& op,
                    decltype(rhs_exprs) const& rhs_exprs,
                    bool const b = false) noexcept
        : statement(), assignees(assignees), op(op), rhs_exprs(rhs_exprs), rhs_tuple_expansion(b)
    {}

    std::string to_string() const noexcept override
    {
        return "ASSIGNMENT_STMT";
    }
};

struct if_stmt final : public statement {
    using elseif_type
        = std::pair<node::any_expr, node::statement_block>;

    symbol::if_kind kind;
    node::any_expr condition;
    node::statement_block then_stmts;
    std::vector<elseif_type> elseif_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;
    bool is_toplevel;

    if_stmt(symbol::if_kind const kind,
            node::any_expr const& cond,
            node::statement_block const& then,
            decltype(elseif_stmts_list) const& elseifs,
            decltype(maybe_else_stmts) const& maybe_else,
            bool const toplevel = true)
        : statement()
        , kind(kind)
        , condition(cond)
        , then_stmts(then)
        , elseif_stmts_list(elseifs)
        , maybe_else_stmts(maybe_else)
        , is_toplevel(toplevel)
    {}

    std::string to_string() const noexcept override
    {
        return "IF_STMT: " + symbol::to_string(kind);
    }
};

struct return_stmt final : public statement {
    std::vector<node::any_expr> ret_exprs;
    type::type ret_type;

    explicit return_stmt(std::vector<node::any_expr> const& rets) noexcept
        : statement(), ret_exprs(rets)
    {}

    explicit return_stmt(node::any_expr const& ret) noexcept
        : statement(), ret_exprs({ret})
    {}

    std::string to_string() const noexcept override
    {
        return "RETURN_STMT";
    }
};

struct case_stmt final : public statement {
    using when_type
        = std::pair<node::any_expr, node::statement_block>;

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
            std::vector<node::any_expr>,
            node::statement_block
        >;

    node::any_expr target_expr;
    std::vector<when_type> when_stmts_list;
    boost::optional<node::statement_block> maybe_else_stmts;
    std::vector<std::vector<scope::weak_func_scope>> when_callee_scopes;

    switch_stmt(node::any_expr const& target,
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
    node::any_expr range_expr;
    node::statement_block body_stmts;
    scope::weak_func_scope index_callee_scope;
    scope::weak_func_scope size_callee_scope;

    for_stmt(decltype(iter_vars) const& iters,
             node::any_expr const& range,
             node::statement_block body) noexcept
        : statement(), iter_vars(iters), range_expr(range), body_stmts(body)
    {}

    std::string to_string() const noexcept override
    {
        return "FOR_STMT";
    }
};

struct while_stmt final : public statement {
    node::any_expr condition;
    node::statement_block body_stmts;

    while_stmt(node::any_expr const& cond,
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
          , node::any_expr
        >;
    body_type body;
    symbol::if_kind kind;
    node::any_expr condition;

    template<class T>
    postfix_if_stmt(T const& body,
                    symbol::if_kind const kind,
                    node::any_expr const& cond) noexcept
        : statement(), body(body), kind(kind), condition(cond)
    {}

    std::string to_string() const noexcept override
    {
        return "POSTFIX_IF_STMT: " + symbol::to_string(kind);
    }
};

struct statement_block final : public base {
    using block_type
        = std::vector<node::compound_stmt>;

    block_type value;
    scope::weak_local_scope scope;

    statement_block() = default;

    explicit statement_block(block_type const& v) noexcept
        : value(v)
    {}

    explicit statement_block(boost::optional<block_type> const& ov) noexcept
        : value(ov ? *ov : block_type{})
    {}

    explicit statement_block(node::compound_stmt const& s) noexcept
        : value({s})
    {}

    std::string to_string() const noexcept override
    {
        return "STATEMENT_BLOCK: size=" + std::to_string(value.size());
    }
};

struct function_definition final : public statement {

    struct ctor_tag {};
    struct copier_tag {};
    struct converter_tag {};

    symbol::func_kind kind;
    std::string name;
    std::vector<node::parameter> params;
    boost::optional<node::any_type> return_type;
    node::statement_block body;
    boost::optional<node::statement_block> ensure_body;
    scope::weak_func_scope scope;
    boost::optional<type::type> ret_type;
    std::vector<node::function_definition> instantiated; // Note: This is not a part of AST!
    boost::optional<bool> accessibility = boost::none;
    boost::optional<bool> is_template_memo = boost::none;

    function_definition(
            symbol::func_kind const k
          , std::string const& n
          , decltype(params) const& p
          , decltype(return_type) const& ret
          , node::statement_block const& block
          , decltype(ensure_body) const& ensure
          , boost::optional<bool> const accessibility = boost::none
    ) noexcept
        : statement()
        , kind(k)
        , name(n)
        , params(p)
        , return_type(ret)
        , body(block)
        , ensure_body(ensure)
        , accessibility(accessibility)
    {}

    // Note:
    // For lambda expression and do-end block
    function_definition(
            decltype(params) const& p,
            node::statement_block const& b,
            decltype(return_type) const& r = boost::none
    ) noexcept
        : statement()
        , kind(symbol::func_kind::func)
        , name("")
        , params(p)
        , return_type(r)
        , body(b)
        , ensure_body(boost::none)
    {}

    // Note:
    // For constructor
    function_definition(ctor_tag, decltype(params) const& p, node::statement_block const& b) noexcept
        : function_definition(
                symbol::func_kind::func,
                "dachs.init",
                p,
                boost::none,
                b,
                boost::none
            )
    {}

    // Note:
    // For copier
    function_definition(copier_tag, node::statement_block const& b) noexcept
        : function_definition(
                symbol::func_kind::func,
                "dachs.copy",
                {},
                boost::none,
                b,
                boost::none
            )
    {}

    // Note:
    // For converter
    function_definition(
            converter_tag,
            decltype(params) const& p,
            node::any_type const& r,
            node::statement_block const& b
    ) noexcept
        : function_definition(
                symbol::func_kind::func,
                "dachs.conv",
                p,
                r,
                b,
                boost::none
            )
    {}

    bool is_template() noexcept;

    bool is_public() const noexcept
    {
        // Note:
        // At the moment, all non-method functions are public.
        return accessibility ? *accessibility : true;
    }

    bool is_ctor() const noexcept
    {
        return name == "dachs.init";
    }

    bool is_copier() const noexcept
    {
        return name == "dachs.copy";
    }

    bool is_converter() const noexcept
    {
        return name == "dachs.conv";
    }

    bool is_main_func() const noexcept
    {
        return name == "main" && (params.empty() || !params[0]->is_receiver);
    }

    std::string to_string() const noexcept override
    {
        return "FUNC_DEFINITION: "
            + symbol::to_string(kind)
            + ' ' + name
            + (
                accessibility ?
                    (*accessibility ?
                        " (public)"
                      : " (private)")
                  : ""
            );
    }
};

struct class_definition final : public statement {
    std::string name;
    std::vector<node::variable_decl> instance_vars;
    std::vector<node::function_definition> member_funcs;
    scope::weak_class_scope scope;
    std::vector<node::class_definition> instantiated; // Note: This is not a part of AST.
    boost::optional<bool> is_template_memo = boost::none;

    class_definition(
            std::string const& n,
            decltype(instance_vars) const& v,
            decltype(member_funcs) const& m
    ) noexcept
        : name(n)
        , instance_vars(v)
        , member_funcs(m)
    {}

    bool is_template() noexcept
    {
        if (!is_template_memo) {
            is_template_memo =
                any_of(
                    instance_vars,
                    [](auto const& i)
                    {
                        assert(!i->symbol.expired());
                        auto const sym = i->symbol.lock();
                        assert(sym->type);
                        return sym->type.is_template();
                    }
                );
        }

        return *is_template_memo;
    }

    std::string to_string() const noexcept override
    {
        return "CLASS_DEFINITION: " + name;
    }
};

struct import final : public base {
    std::string path;

    template<class String>
    explicit import(String && p)
        : path(std::forward<String>(p))
    {}

    std::string to_string() const noexcept override
    {
        return "IMPORT: " + path;
    }
};

struct inu final : public base {
    std::vector<node::function_definition> functions;
    std::vector<node::initialize_stmt> global_constants;
    std::vector<node::class_definition> classes;
    std::vector<node::import> imports;

    inu(
        decltype(functions) const& fs,
        decltype(global_constants) const& gs,
        decltype(classes) const& cs,
        decltype(imports) const& is
    ) noexcept
        : base()
        , functions(fs)
        , global_constants(gs)
        , classes(cs)
        , imports(is)
    {}

    std::string to_string() const noexcept override
    {
        return "PROGRAM: functions: " + std::to_string(functions.size())
            + ", constants: " + std::to_string(global_constants.size())
            + ", imports: " + std::to_string(imports.size());
    }
};

} // namespace node_type

struct ast {
    node::inu root;
    std::string const name;
};

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_HPP_INCLUDED
