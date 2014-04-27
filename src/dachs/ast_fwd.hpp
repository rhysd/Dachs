#if !defined DACHS_AST_FWD_HPP_INCLUDED
#define      DACHS_AST_FWD_HPP_INCLUDED

#include <memory>
#include <cassert>

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

// Forward class declarations
struct base;
struct integer_literal;
struct character_literal;
struct float_literal;
struct boolean_literal;
struct string_literal;
struct array_literal;
struct tuple_literal;
struct symbol_literal;
struct dict_literal;
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
struct dict_type;
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
struct constant_decl;
struct constant_definition;
struct global_definition;
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
DACHS_DEFINE_NODE_PTR(dict_literal);
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
DACHS_DEFINE_NODE_PTR(dict_type);
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
DACHS_DEFINE_NODE_PTR(global_definition);
DACHS_DEFINE_NODE_PTR(constant_decl);
DACHS_DEFINE_NODE_PTR(constant_definition);
DACHS_DEFINE_NODE_PTR(program);
#undef DACHS_DEFINE_NODE_PTR

} // namespace node


namespace node {
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

    std::shared_ptr<node_type::base> get_shared() const noexcept
    {
        assert(!node.expired());
        return node.lock();
    }

    template<class T>
    boost::optional<std::shared_ptr<T>> get_shared_as() const noexcept
    {
        assert(!node.expired());
        auto const shared = std::dynamic_pointer_cast<T>(node.lock());
        return shared ? shared : boost::none;
    }
};
} // namespace node

} // namespace ast
} // namespace dachs

#endif    // DACHS_AST_FWD_HPP_INCLUDED
