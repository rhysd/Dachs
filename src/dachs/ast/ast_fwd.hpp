#if !defined DACHS_AST_AST_FWD_HPP_INCLUDED
#define      DACHS_AST_AST_FWD_HPP_INCLUDED

#include <memory>
#include <type_traits>
#include <tuple>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/optional.hpp>

namespace dachs {
namespace ast {

namespace location {

enum location_index {
    line = 0,
    col = 1,
    length = 2,
};

} // namespace

using location_type = std::tuple<std::size_t, std::size_t, std::size_t>;

namespace symbol {

enum class if_kind {
    if_,
    unless,
};

enum class qualifier {
    maybe,
};

enum class func_kind {
    func,
    proc,
    method,
};

std::string to_string(if_kind const o);
std::string to_string(qualifier const o);
std::string to_string(func_kind const o);

} // namespace symbol

namespace node_type {

std::size_t generate_id() noexcept;

struct base {
    base() noexcept
        : id(generate_id())
    {}

    void set_source_location(location_type const& l) noexcept
    {
        line = std::get<location::line>(l);
        col = std::get<location::col>(l);
        length = std::get<location::length>(l);
    }

    template<class Node>
    void set_source_location(Node const& n) noexcept
    {
        line = n.line;
        col = n.col;
        length = n.length;
    }

    auto source_location() const noexcept
    {
        return std::make_tuple(line, col, length);
    }

    virtual ~base() noexcept
    {}

    virtual std::string to_string() const noexcept = 0;

    std::size_t line = 0, col = 0, length = 0;
    std::size_t id;
};

struct expression;
struct statement;

// Forward class declarations
struct primary_literal;
struct symbol_literal;
struct array_literal;
struct tuple_literal;
struct dict_literal;
struct lambda_expr;
struct var_ref;
struct parameter;
struct func_invocation;
struct object_construct;
struct index_access;
struct ufcs_invocation;
struct unary_expr;
struct binary_expr;
struct primary_type;
struct tuple_type;
struct func_type;
struct array_type;
struct dict_type;
struct qualified_type;
struct cast_expr;
struct typed_expr;
struct if_expr;
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
struct let_stmt;
struct statement_block;
struct function_definition;
struct class_definition;
struct inu;

}

namespace traits {

namespace detail {

template<class T, class U>
struct is_base_or_itself
    : std::integral_constant<
        bool,
        std::is_base_of<T, typename std::remove_cv<U>::type>::value ||
        std::is_same<T, typename std::remove_cv<U>::type>::value
    > {};
} // namespace detail

template<class T>
struct is_node : detail::is_base_or_itself<node_type::base, T> {};

template<class T>
struct is_expression : detail::is_base_or_itself<node_type::expression, T> {};

template<class T>
struct is_statement : detail::is_base_or_itself<node_type::statement, T> {};

} // namespace traits

namespace node {

#define DACHS_DEFINE_NODE_PTR(n) using n = std::shared_ptr<node_type::n>
DACHS_DEFINE_NODE_PTR(base);
DACHS_DEFINE_NODE_PTR(primary_literal);
DACHS_DEFINE_NODE_PTR(symbol_literal);
DACHS_DEFINE_NODE_PTR(array_literal);
DACHS_DEFINE_NODE_PTR(tuple_literal);
DACHS_DEFINE_NODE_PTR(dict_literal);
DACHS_DEFINE_NODE_PTR(lambda_expr);
DACHS_DEFINE_NODE_PTR(var_ref);
DACHS_DEFINE_NODE_PTR(parameter);
DACHS_DEFINE_NODE_PTR(func_invocation);
DACHS_DEFINE_NODE_PTR(object_construct);
DACHS_DEFINE_NODE_PTR(index_access);
DACHS_DEFINE_NODE_PTR(ufcs_invocation);
DACHS_DEFINE_NODE_PTR(unary_expr);
DACHS_DEFINE_NODE_PTR(binary_expr);
DACHS_DEFINE_NODE_PTR(cast_expr);
DACHS_DEFINE_NODE_PTR(typed_expr);
DACHS_DEFINE_NODE_PTR(if_expr);
DACHS_DEFINE_NODE_PTR(primary_type);
DACHS_DEFINE_NODE_PTR(tuple_type);
DACHS_DEFINE_NODE_PTR(func_type);
DACHS_DEFINE_NODE_PTR(array_type);
DACHS_DEFINE_NODE_PTR(dict_type);
DACHS_DEFINE_NODE_PTR(qualified_type);
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
DACHS_DEFINE_NODE_PTR(let_stmt);
DACHS_DEFINE_NODE_PTR(statement_block);
DACHS_DEFINE_NODE_PTR(function_definition);
DACHS_DEFINE_NODE_PTR(class_definition);
DACHS_DEFINE_NODE_PTR(inu);
#undef DACHS_DEFINE_NODE_PTR

using any_expr =
    boost::variant< typed_expr
                  , primary_literal
                  , symbol_literal
                  , array_literal
                  , dict_literal
                  , tuple_literal
                  , lambda_expr
                  , ufcs_invocation
                  , index_access
                  , func_invocation
                  , object_construct
                  , unary_expr
                  , binary_expr
                  , cast_expr
                  , if_expr
                  , var_ref
            >;

using any_type =
    boost::variant< qualified_type
                  , tuple_type
                  , func_type
                  , array_type
                  , dict_type
                  , primary_type
                >;

using compound_stmt =
    boost::variant<
          if_stmt
        , return_stmt
        , case_stmt
        , switch_stmt
        , for_stmt
        , while_stmt
        , assignment_stmt
        , initialize_stmt
        , postfix_if_stmt
        , let_stmt
        , statement_block
        , any_expr
    >;

class any_node {
    std::weak_ptr<node_type::base> node;

public:

    explicit any_node(std::nullptr_t) noexcept
        : node{}
    {}

    template<class Ptr>
    explicit any_node(Ptr const& p) noexcept
        : node{p}
    {}

    any_node() noexcept
        : node{}
    {}

    bool empty() const noexcept
    {
        return node.expired();
    }

    void set_node(std::nullptr_t) noexcept
    {
        node = decltype(node){};
    }

    template<class Ptr>
    void set_node(Ptr const& n) noexcept
    {
        node = n;
    }

    decltype(node) get_weak() const noexcept
    {
        return node;
    }

    node::base get_shared() const noexcept
    {
        assert(!node.expired());
        return node.lock();
    }

    bool is_root() const noexcept
    {
        if (node.expired()) {
            return false;
        }
        return bool{std::dynamic_pointer_cast<node::inu>(node.lock())};
    }

};

// Note: Use free function for the same reason as std::get<>()
template<class NodePtr>
inline boost::optional<NodePtr> get_shared_as(any_node const& node) noexcept
{
    using Node = typename NodePtr::element_type;
    static_assert(traits::is_node<Node>::value, "ast::node::get_shared_as(): Not an AST node.");

    auto const shared = std::dynamic_pointer_cast<Node>(node.get_shared());
    if (shared) {
        return {shared};
    } else {
        return boost::none;
    }
}

template<class NodePtr>
inline bool is_a(any_node const& node) noexcept
{
    using Node = typename NodePtr::element_type;
    static_assert(traits::is_node<Node>::value, "ast::node::is_a(): Not an AST node.");
    return {dynamic_cast<Node *>(node.get_shared().get())};
}

} // namespace node

struct ast;

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_AST_FWD_HPP_INCLUDED
