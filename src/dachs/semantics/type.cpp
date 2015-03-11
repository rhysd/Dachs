#include <cassert>
#include <cstddef>

#include <boost/optional.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace type {
using helper::variant::get_as;

namespace detail {

using helper::variant::apply_lambda;
using std::size_t;
using boost::adaptors::transformed;

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

template<class T>
inline auto instance_var_types_of(T const& t)
    -> boost::optional<std::vector<any_type>>
{
    auto const scope = t.ref.lock();
    std::vector<any_type> ret;

    if (t.param_types.empty()) {
        for (auto const& i : scope->instance_var_symbols) {
            ret.push_back(i->type);
        }
    } else {
        auto itr = std::begin(t.param_types);
        auto const end = std::end(t.param_types);
        for (auto const& i : scope->instance_var_symbols) {
            if (is_a<template_type>(i->type)) {
                if (itr == end) {
                    return boost::none;
                }

                ret.push_back(*itr);

                ++itr;
            } else {
                ret.push_back(i->type);
            }
        }
        if (itr != end) {
            return boost::none;
        }
    }
    return ret;
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
        }

        // TODO:
        // Template types must be considered.
        auto const c = apply_lambda([&t](auto const& s){ return s->resolve_class(t->template_name); }, current_scope);
        if (!c) {
            return {};
        }

        return make<class_type>(
                *c,
                t->holders | transformed(
                    [this](auto const& i){ return apply_recursively(i); }
                    )
            );
    }

    any_type operator()(ast::node::array_type const& t) const noexcept
    {
        if (t->elem_type) {
            return make<array_type>(
                        apply_recursively(*t->elem_type)
                    );
        } else {
            return make<array_type>(
                        make<template_type>(t)
                    );
        }
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

    explicit type_to_node_translator(ast::location_type const& l) noexcept
        : location(l)
    {}

    result_type operator()(builtin_type const& t) const
    {
        return make<ast::node::primary_type>(t->name);
    }

    result_type operator()(class_type const& t) const
    {
        // XXX:
        // This implementation is not sufficient.
        // If the type 't' is a class instantiated from class template,
        // it can't set information of instantiation to the result AST node.
        // (e.g. what types it is instantiated with?)
        // To implement it, at first look up the class by name and check the
        // result type is template or not.  If template, check the difference
        // between the result type and the class type 't'.  By knowing the
        // difference,  I can know what types are set to instantiate the class
        // type 't'.  Then I can set the types to the result type of this function.
        auto instantiated = prepare_vector(t->param_types);
        for (auto const& p : t->param_types) {
            instantiated.push_back(apply_recursively(p));
        }

        return make<ast::node::primary_type>(t->name, instantiated);
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

struct instantiation_checker : boost::static_visitor<bool> {

    result_type visit(any_type const& l, any_type const& r) const noexcept
    {
        return boost::apply_visitor(*this, l.raw_value(), r.raw_value());
    }

    result_type visit(std::vector<any_type> const& ls, std::vector<any_type> const& rs) const noexcept
    {
        for (auto const& lr : helper::zipped(ls, rs)) {
            if (visit(boost::get<0>(lr), boost::get<1>(lr))) {
                return true;
            }
        }
        return false;
    }

    result_type operator()(class_type const& l, class_type const& r) const noexcept
    {
        if (l->name != r->name) {
            return false;
        }

        auto const ls = l->ref.lock();
        auto const rs = r->ref.lock();

        if (l->is_template()) {
            return false;
        }

        auto const lts = detail::instance_var_types_of(*l);
        auto const rts = detail::instance_var_types_of(*r);
        if (!lts || !rts) {
            return false;
        }

        assert(lts->size() == rts->size());
        for (auto const& lr : helper::zipped(*lts, *rts)) {
            if (!visit(boost::get<0>(lr), boost::get<1>(lr))) {
                return false;
            }
        }

        return true;
    }

    result_type operator()(tuple_type const& l, tuple_type const& r) const noexcept
    {
        return visit(l->element_types, r->element_types);
    }

    result_type operator()(func_type const& l, func_type const& r) const noexcept
    {
        return visit(l->return_type, r->return_type) || visit(l->param_types, r->param_types);
    }

    result_type operator()(dict_type const& l, dict_type const& r) const noexcept
    {
        return visit(l->key_type, r->key_type) || visit(l->value_type, r->value_type);
    }

    result_type operator()(array_type const& l, array_type const& r) const noexcept
    {
        return visit(l->element_type, r->element_type);
    }

    result_type operator()(range_type const& l, range_type const& r) const noexcept
    {
        return visit(l->element_type, r->element_type);
    }

    result_type operator()(qualified_type const& l, qualified_type const& r) const noexcept
    {
        return visit(l->contained_type, r->contained_type);
    }

    template<class T>
    result_type operator()(T const&, template_type const&) const noexcept
    {
        return true;
    }

    template<class T, class U>
    result_type operator()(T const& l, U const& r) const noexcept
    {
        return *l == *r;
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

bool any_type::is_unit() const noexcept
{
    auto const t = get_as<tuple_type>(value);
    if (!t) {
        return false;
    }

    return (*t)->element_types.empty();
}

bool any_type::is_array_class() const noexcept
{
    auto const c = get_as<class_type>(value);
    return c && (*c)->name == "array";
}

boost::optional<array_type const&> any_type::get_array_underlying_type() const noexcept
{
    auto const c = get_as<class_type>(value);
    if (!c || (*c)->name != "array") {
        return boost::none;
    }

    if ((*c)->param_types.size() == 1) {
        return get<array_type>((*c)->param_types[0]);
    } else if ((*c)->param_types.empty() && !(*c)->ref.expired()) {
        auto const scope = (*c)->ref.lock();
        auto const& syms = scope->instance_var_symbols;
        if (syms.size() == 3 /*buf, capacity, size*/) {
            return get<array_type>((*c)->param_types[0]);
        }
    }

    return boost::none;
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
    if (auto const c = get_as<class_type>(value)) {
        // Note: When the type is a class, check if it is class template
        return (*c)->is_template();
    } else if (auto const a = get_as<array_type>(value)){
        return (*a)->element_type.is_template();
    } else {
        return helper::variant::has<template_type>(value);
    }
}

bool any_type::is_class_template() const noexcept
{
    if (auto const maybe_array = get_as<array_type>(value)) {
        return (*maybe_array)->element_type.is_class_template();
    }

    auto const maybe_type = get_as<class_type>(value);
    if (!maybe_type) {
        return false;
    }

    auto const& c = *maybe_type;
    if (!c->param_types.empty()) {
        // Note:
        // If the class has any specified template parameter,
        // it means the class template was already instantiated.
        for (auto const& t : c->param_types) {
            if (t.is_class_template()) {
                return true;
            }
        }

        return false;
    }

    assert(!c->ref.expired());
    return c->ref.lock()->is_template();
}

bool is_instantiated_from(class_type const& instantiated_class, class_type const& template_class)
{
    if (!template_class->is_template()) {
        return false;
    }
    detail::instantiation_checker checker;
    return checker(instantiated_class, template_class);
}

bool is_instantiated_from(array_type const& instantiated_array, array_type const& template_array)
{
    if (!template_array->element_type.is_template()) {
        return false;
    }
    detail::instantiation_checker checker;
    return checker(instantiated_array, template_array);
}

bool any_type::is_instantiated_from(class_type const& from) const
{
    if (auto const c = get_as<class_type>(value)) {
        return ::dachs::type::is_instantiated_from(*c, from);
    } else {
        return false;
    }
}

bool any_type::is_instantiated_from(array_type const& from) const
{
    if (auto const a = get_as<array_type>(value)) {
        return ::dachs::type::is_instantiated_from(*a, from);
    } else {
        return false;
    }
}

any_type from_ast(ast::node::any_type const& t, scope::any_scope const& current) noexcept
{
    return boost::apply_visitor(detail::node_to_type_translator{current}, t);
}

ast::node::any_type to_ast(any_type const& t, ast::location_type && location) noexcept
{
    return boost::apply_visitor(detail::type_to_node_translator{std::move(location)}, t);
}

ast::node::any_type to_ast(any_type const& t, ast::location_type const& location) noexcept
{
    return boost::apply_visitor(detail::type_to_node_translator{location}, t);
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

template<class Types>
class_type::class_type(scope::class_scope const& s, Types const& types) noexcept
    : class_type(s)
{
    for (auto const& t : types) {
        param_types.push_back(t);
    }
}

bool class_type::operator==(class_type const& rhs) const noexcept
{
    assert(!ref.expired() && !rhs.ref.expired());

    if (name != rhs.name) {
        return false;
    }


    auto const maybe_lhs_types = type::detail::instance_var_types_of(*this);
    auto const maybe_rhs_types = type::detail::instance_var_types_of(rhs);

    if (!maybe_lhs_types || !maybe_rhs_types || (maybe_lhs_types->size() != maybe_rhs_types->size())) {
        // Note: Error
        return false;
    }

    for (auto const& lr : helper::zipped(*maybe_lhs_types, *maybe_rhs_types)) {
        auto const& l = boost::get<0>(lr);
        auto const& r = boost::get<1>(lr);
        if (type::is_a<type::template_type>(l) && type::is_a<type::template_type>(r)) {
            continue;
        }
        if (l != r) {
            return false;
        }
    }

    return true;
}

std::string class_type::stringize_param_types() const
{
    auto const stringize_params_from_symbols
        = [this]
        {
            auto const scope = ref.lock();
            return boost::algorithm::join(
                scope->instance_var_symbols
                    | transformed(
                        [](auto const& s)
                        {
                            return !type::is_a<type::template_type>(s->type)
                                ? s->type.to_string()
                                : "T";
                        }
                    ),
                ", "
            );
        };

    auto const stringize_params_from_params
        = [this]
        {
            return boost::algorithm::join(
                param_types
                    | transformed(
                        [](auto const& t)
                        {
                            assert(!type::is_a<type::template_type>(t));
                            return t.to_string();
                        }
                    ),
                ", "
            );
        };

    std::string const ret
        = param_types.empty()
            ? stringize_params_from_symbols()
            : stringize_params_from_params();

    return ret.empty() ? ret : '(' + ret + ')';
}

bool class_type::is_template() const
{
    if (param_types.empty()) {
        assert(!ref.expired());
        return ref.lock()->is_template();
    } else {
        return boost::algorithm::any_of(
                param_types,
                [](auto const& t){ return t.is_template(); }
            );
    }
}

std::string class_type::to_string() const noexcept
{
    assert(!ref.expired());
    return "class " + name + stringize_param_types();
}

bool class_type::is_default_constructible() const noexcept
{
    assert(!ref.expired());
    auto const ctor_candidates = ref.lock()->resolve_ctor(
            {
                type::any_type{type::make<type::class_type>(*this)}
            }
        );
    if (ctor_candidates.size() != 1u) {
        return false;
    }

    auto itr = param_types.cbegin();
    for (auto const& s : ref.lock()->instance_var_symbols) {
        if (s->type.is_template()) {
            if (itr == param_types.cend()) {
                return false;
            }

            if (!itr->is_default_constructible()) {
                return false;
            }

            ++itr;
        } else if (!s->type.is_default_constructible()) {
            return false;
        }
    }

    return true;
}

} // namespace type_node
} // namespace dachs
