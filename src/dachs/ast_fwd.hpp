#if !defined DACHS_AST_FWD_HPP_INCLUDED
#define      DACHS_AST_FWD_HPP_INCLUDED

#include <memory>
#include <type_traits>
#include <cassert>

namespace dachs {
namespace ast {

namespace symbol {

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

enum class func_kind {
    func,
    proc,
};

std::string to_string(assign_operator const o);
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
struct var_ref;
struct parameter;
struct func_invocation;
struct object_construct;
struct index_access;
struct member_access;
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
struct statement_block;
struct function_definition;
struct constant_decl;
struct constant_definition;
struct program;

}

namespace traits {

template<class T>
struct is_node : std::is_base_of<node_type::base, typename std::remove_cv<T>::type> {};

template<class T>
struct is_expression : std::is_base_of<node_type::expression, typename std::remove_cv<T>::type> {};

template<class T>
struct is_statement : std::is_base_of<node_type::statement, typename std::remove_cv<T>::type> {};

} // namespace traits

namespace node {

#define DACHS_DEFINE_NODE_PTR(n) using n = std::shared_ptr<node_type::n>
DACHS_DEFINE_NODE_PTR(base);
DACHS_DEFINE_NODE_PTR(primary_literal);
DACHS_DEFINE_NODE_PTR(symbol_literal);
DACHS_DEFINE_NODE_PTR(array_literal);
DACHS_DEFINE_NODE_PTR(tuple_literal);
DACHS_DEFINE_NODE_PTR(dict_literal);
DACHS_DEFINE_NODE_PTR(var_ref);
DACHS_DEFINE_NODE_PTR(parameter);
DACHS_DEFINE_NODE_PTR(func_invocation);
DACHS_DEFINE_NODE_PTR(object_construct);
DACHS_DEFINE_NODE_PTR(index_access);
DACHS_DEFINE_NODE_PTR(member_access);
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
DACHS_DEFINE_NODE_PTR(statement_block);
DACHS_DEFINE_NODE_PTR(function_definition);
DACHS_DEFINE_NODE_PTR(constant_decl);
DACHS_DEFINE_NODE_PTR(constant_definition);
DACHS_DEFINE_NODE_PTR(program);
#undef DACHS_DEFINE_NODE_PTR

using any_expr =
    boost::variant< typed_expr
                  , primary_literal
                  , symbol_literal
                  , array_literal
                  , dict_literal
                  , tuple_literal
                  , member_access
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
        , any_expr
    >;

using global_definition =
    boost::variant<
        function_definition,
        constant_definition
    >;

class any_node {
    std::weak_ptr<node_type::base> node;

public:

    template<class Ptr>
    explicit any_node(Ptr const& p) noexcept
        : node{p}
    {
        assert(!node.expired());
    }

    any_node() noexcept
        : node{}
    {}

    bool empty() const noexcept
    {
        return node.expired();
    }

    template<class Ptr>
    void set_node(Ptr const& n) noexcept
    {
        assert(node.expired());
        node = n;
        assert(!node.expired());
    }

    decltype(node) get_weak() const noexcept
    {
        assert(!node.expired());
        return node;
    }

    node::base get_shared() const noexcept
    {
        assert(!node.expired());
        return node.lock();
    }

    bool is_root() const noexcept
    {
        assert(!node.expired());
        return bool{std::dynamic_pointer_cast<node::program>(node.lock())};
    }

    template<class T>
    boost::optional<std::shared_ptr<T>> get_shared_as() const noexcept
    {
        static_assert(traits::is_node<T>::value, "any_node::get_shared_as(): T is not AST node.");
        assert(!node.expired());
        auto const shared = std::dynamic_pointer_cast<T>(node.lock());
        return shared ? shared : boost::none;
    }
};
} // namespace node

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_FWD_HPP_INCLUDED
