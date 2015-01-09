#include <cassert>
#include <cstddef>

#include <boost/optional.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace type {
namespace detail {

using helper::variant::apply_lambda;
using std::size_t;

static std::vector<builtin_type> const builtin_types
    = {
        make<builtin_type>("int"),
        make<builtin_type>("uint"),
        make<builtin_type>("float"),
        make<builtin_type>("char"),
        make<builtin_type>("bool"),
        make<builtin_type>("string"),
        make<builtin_type>("symbol"),
    };

class node_to_type_translator
    : public boost::static_visitor<any_type> {

    scope::any_scope const& current_scope;

    template<class T>
    any_type apply_recursively(T const& t) const noexcept
    {
        return boost::apply_visitor(*this, t);
    }

public:

    explicit node_to_type_translator(scope::any_scope const& c)
        : current_scope(c)
    {}

    any_type operator()(ast::node::primary_type const& t) const noexcept
    {
        auto const builtin = get_builtin_type(t->template_name.c_str());
        if (builtin) {
            return *builtin;
        } else {
            // TODO:
            // Template types must be considered.
            auto const c = apply_lambda([&t](auto const& s){ return s->resolve_class(t->template_name); }, current_scope);
            if (!c) {
                return {};
            }

            return make<class_type>(*c);
        }
    }

    any_type operator()(ast::node::array_type const& t) const noexcept
    {
        return make<array_type>(
                    apply_recursively(t->elem_type)
                );
    }

    any_type operator()(ast::node::tuple_type const& t) const noexcept
    {
        auto const ret = make<tuple_type>();
        ret->element_types.reserve(t->arg_types.size());
        for (auto const& arg : t->arg_types) {
            ret->element_types.push_back(apply_recursively(arg));
        }
        return ret;
    }

    any_type operator()(ast::node::dict_type const& t) const noexcept
    {
        return make<dict_type>(
                    apply_recursively(t->key_type),
                    apply_recursively(t->value_type)
                );
    }

    any_type operator()(ast::node::qualified_type const& t) const noexcept
    {
        qualifier new_qualifier;
        switch (t->qualifier) {
        case ast::symbol::qualifier::maybe:
            new_qualifier = qualifier::maybe;
            break;
        default:
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        return make<qualified_type>(
                    new_qualifier, apply_recursively(t->type)
               );
    }

    any_type operator()(ast::node::func_type const& t) const noexcept
    {
        std::vector<any_type> param_types;
        param_types.reserve(t->arg_types.size());
        for (auto const& a : t->arg_types) {
            param_types.push_back(apply_recursively(a));
        }

        if (t->ret_type) {
            return {make<func_type>(
                    std::move(param_types),
                    apply_recursively(*(t->ret_type))
                )};
        } else {
            return {make<func_type>(std::move(param_types), get_unit_type(), ast::symbol::func_kind::proc)};
        }
    }
};

class type_to_node_translator
    : public boost::static_visitor<ast::node::any_type> {

    ast::location_type location;

    template<class Ptr, class... Args>
    Ptr make(Args &&... args) const
    {
        auto const node = helper::make<Ptr>(std::forward<Args>(args)...);
        set_location(node);
        return node;
    }

    template<class Node>
    void set_location(Node &n) const noexcept
    {
        n->set_source_location(location);
    }

    template<class T>
    result_type apply_recursively(T const& t) const noexcept
    {
        return boost::apply_visitor(*this, t);
    }

    auto prepare_vector(size_t const size) const
    {
        std::vector<result_type> v;
        v.reserve(size);
        return v;
    }

    template<class... Args>
    auto prepare_vector(std::vector<Args...> const& hint) const
    {
        return prepare_vector(hint.size());
    }

public:

    explicit type_to_node_translator(ast::location_type && l) noexcept
        : location(std::move(l))
    {}

    result_type operator()(builtin_type const& t) const
    {
        return make<ast::node::primary_type>(t->name);
    }

    result_type operator()(class_type const& t) const
    {
        assert(!t->ref.expired());
        auto const scope = t->ref.lock();
        auto instantiated = prepare_vector(scope->instance_var_symbols);
        for (auto const& i : scope->instance_var_symbols) {
            instantiated.push_back(apply_recursively(i->type));
        }

        return make<ast::node::primary_type>(scope->name, instantiated);
    }

    result_type operator()(tuple_type const& t) const
    {
        auto elem_type_nodes = prepare_vector(t->element_types);
        for (auto const& e : t->element_types) {
            elem_type_nodes.push_back(apply_recursively(e));
        }

        return make<ast::node::tuple_type>(elem_type_nodes);
    }

    result_type operator()(func_type const& t) const
    {
        auto param_type_nodes = prepare_vector(t->param_types);
        for (auto const& p : t->param_types) {
            param_type_nodes.push_back(apply_recursively(p));
        }

        return make<ast::node::func_type>(param_type_nodes, apply_recursively(t->return_type));
    }

    result_type operator()(generic_func_type const& t) const
    {
        assert(t->ref);
        auto const scope = t->ref->lock();
        auto param_type_nodes = prepare_vector(scope->params);
        for (auto const& p : scope->params) {
            param_type_nodes.push_back(apply_recursively(p->type));
        }

        if (scope->ret_type) {
            return make<ast::node::func_type>(param_type_nodes, apply_recursively(*scope->ret_type));
        } else {
            return make<ast::node::func_type>(param_type_nodes);
        }
    }

    result_type operator()(dict_type const& t) const
    {
        return make<ast::node::dict_type>(
                    apply_recursively(t->key_type),
                    apply_recursively(t->value_type)
                );
    }

    result_type operator()(array_type const& t) const
    {
        return make<ast::node::array_type>(apply_recursively(t->element_type));
    }

    result_type operator()(range_type const& t) const
    {
        // TODO
        std::vector<result_type> elem_type_node = {apply_recursively(t->element_type)};
        return make<ast::node::primary_type>("range", elem_type_node);
    }

    result_type operator()(qualified_type const& t) const
    {
        switch (t->qualifier) {
        case qualifier::maybe:
            return make<ast::node::qualified_type>(ast::symbol::qualifier::maybe, apply_recursively(t->contained_type));
        default:
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    result_type operator()(template_type const&) const
    {
        assert(false && "AST node for template type doesn't exist");
        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }
};

} // namespace detail

no_opt_t no_opt;

boost::optional<builtin_type> get_builtin_type(char const* const name) noexcept
{
    for (auto const& t : detail::builtin_types) {
        if (t->name == name) {
            return t;
        }
    }

    return boost::none;
}

builtin_type get_builtin_type(char const* const name, no_opt_t) noexcept
{
    for (auto const& t : detail::builtin_types) {
        if (t->name == name) {
            return t;
        }
    }

    DACHS_RAISE_INTERNAL_COMPILATION_ERROR
}

tuple_type const& get_unit_type() noexcept
{
    static auto const unit_type = make<tuple_type>();
    return unit_type;
}

template<class String>
bool any_type::is_builtin(String const& name) const noexcept
{
    auto const t = helper::variant::get_as<builtin_type>(value);
    if (!t) {
        return false;
    }

    return (*t)->name == name;
}

bool any_type::is_unit() const noexcept
{
    auto const t = helper::variant::get_as<tuple_type>(value);
    if (!t) {
        return false;
    }

    return (*t)->element_types.empty();
}

bool any_type::operator==(any_type const& rhs) const noexcept
{
    return helper::variant::apply_lambda(
            [](auto const& l, auto const& r)
            {
                if (!l || !r) {
                    // If both sides are empty, return true.  Otherwise return false.
                    return !l && !r;
                } else {
                    return *l == *r;
                }
            }, value, rhs.value);
}

bool any_type::is_default_constructible() const noexcept
{
    return apply_lambda([](auto const& t){ return t ? t->is_default_constructible() : false; });
}

bool any_type::is_template() const noexcept
{
    if (auto const c = helper::variant::get_as<class_type>(value)) {
        // Note: When the type is a class, check if it is class template
        assert(!(*c)->ref.expired());
        return (*c)->ref.lock()->is_template();
    } else {
        return helper::variant::has<template_type>(value);
    }
}

bool any_type::is_class_template() const noexcept
{
    auto const t = helper::variant::get_as<class_type>(value);
    if (!t) {
        return false;
    }

    auto const& c = *t;
    assert(!c->ref.expired());
    return c->ref.lock()->is_template();
}

any_type from_ast(ast::node::any_type const& t, scope::any_scope const& current) noexcept
{
    return boost::apply_visitor(detail::node_to_type_translator{current}, t);
}

ast::node::any_type to_ast(any_type const& t, ast::location_type && location) noexcept
{
    return boost::apply_visitor(detail::type_to_node_translator{std::move(location)}, t);
}

} // namespace type

namespace type_node {

bool generic_func_type::operator==(generic_func_type const& rhs) const noexcept
{
    if (!ref && !rhs.ref) {
        return true;
    }

    if (ref && rhs.ref) {
        assert(!ref->expired() && !rhs.ref->expired());
        // Note:
        // Compare two pointers because generic function type is equal iff two (instantiated)
        // functions are exactly the same.
        // If compare two scopes directly, it causes infinite loop when the parameter
        // of function has its generic function type.
        return ref->lock() == rhs.ref->lock();
    }

    return false;
}

std::string generic_func_type::to_string() const noexcept
{
    if (!ref || ref->expired()) {
        return "<funcref:UNKNOWN>";
    }

    return "<funcref:" + ref->lock()->name + '>';
}

boost::optional<ast::node::parameter> template_type::get_ast_node_as_parameter() const noexcept
{
    return ast::node::get_shared_as<ast::node::parameter>(ast_node);
}

boost::optional<ast::node::variable_decl> template_type::get_ast_node_as_var_decl() const noexcept
{
    return ast::node::get_shared_as<ast::node::variable_decl>(ast_node);
}

std::string template_type::to_string() const noexcept
{
    auto const node = ast_node.get_shared();
    if (node) {
        return "<template:" + std::to_string(node->line) + ":" + std::to_string(node->col) + ">";
    } else {
        return "<template:UNKNOWN>";
    }
}

class_type::class_type(scope::class_scope const& s) noexcept
    : named_type(s->name), ref(s)
{}

std::string class_type::to_string() const noexcept
{
    if (ref.expired()) {
        return "<class:UNKNOWN>";
    }

    auto const scope = ref.lock();
    if (scope->is_template()) {
        return "<class template:" + name + ':' + helper::hex_string_of_ptr(scope.get()) + '>';
    } else {
        return "<class:" + name + ':' + helper::hex_string_of_ptr(scope.get()) + '>';
    }
}

bool class_type::is_default_constructible() const noexcept
{
    assert(!ref.expired());
    return ref.lock()->resolve_ctor({});
}

} // namespace type_node
} // namespace dachs
