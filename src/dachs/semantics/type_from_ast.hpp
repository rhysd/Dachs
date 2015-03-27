#if !defined DACHS_SEMANTICS_TYPE_FROM_AST_HPP_INCLUDED
#define      DACHS_SEMANTICS_TYPE_FROM_AST_HPP_INCLUDED

#include <boost/optional.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/probable.hpp"

namespace dachs {
namespace type {
namespace detail {

using helper::variant::apply_lambda;
using boost::adaptors::transformed;

template<class Analyzer>
class node_to_type_translator
    : public boost::static_visitor<any_type> {

    scope::any_scope const& current_scope;
    boost::optional<Analyzer &> analyzer = boost::none;
    boost::optional<std::string> failed_name = boost::none;

    template<class T>
    any_type apply_recursively(T const& t)
    {
        return boost::apply_visitor(*this, t);
    }

public:

    explicit node_to_type_translator(scope::any_scope const& c)
        : current_scope(c)
    {}

    node_to_type_translator(scope::any_scope const& c, Analyzer &a)
        : current_scope(c), analyzer(a)
    {}

    boost::optional<std::string> const& failed()
    {
        return failed_name;
    }

    any_type operator()(ast::node::primary_type const& t)
    {
        auto const builtin = get_builtin_type(t->template_name.c_str());
        if (builtin) {
            return *builtin;
        }

        auto const c = apply_lambda([&t](auto const& s){ return s->resolve_class(t->template_name); }, current_scope);
        if (!c) {
            failed_name = t->template_name;
            return {};
        }

        return make<class_type>(
                *c,
                t->holders | transformed(
                    [this](auto const& i){ return apply_recursively(i); }
                    )
            );
    }

    any_type operator()(ast::node::array_type const& t)
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

    any_type operator()(ast::node::pointer_type const& t)
    {
        if (t->pointee_type) {
            return make<pointer_type>(
                        apply_recursively(*t->pointee_type)
                    );
        } else {
            return make<pointer_type>(
                        make<template_type>(t)
                    );
        }
    }

    any_type operator()(ast::node::tuple_type const& t)
    {
        auto const ret = make<tuple_type>();
        ret->element_types.reserve(t->arg_types.size());
        for (auto const& arg : t->arg_types) {
            ret->element_types.push_back(apply_recursively(arg));
        }
        return ret;
    }

    any_type operator()(ast::node::qualified_type const& t)
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

    any_type operator()(ast::node::func_type const& t)
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

    any_type operator()(ast::node::typeof_type const& t)
    {
        if (!analyzer) {
            throw not_implemented_error{__FILE__, __func__, __LINE__, "typeof({expr}) outside the body of function"};
        }

        ast::walk_topdown(t->expr, *analyzer);

        auto const the_type = type_of(t->expr);
        if (!the_type) {
            failed_name = "invalid typeof() use";
        }

        return the_type;
    }

    any_type operator()(ast::node::dict_type const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "dictionary type"};
    }
};

template<class Visitor>
helper::probable<any_type> from_ast_impl(ast::node::any_type const& t, Visitor &&v)
{
    auto const result = boost::apply_visitor(v, t);

    if (v.failed()) {
        return helper::oops(*v.failed());
    }

    return result;
}

} // namespace detail

template<class Analyzer>
helper::probable<any_type> from_ast(ast::node::any_type const& t, scope::any_scope const& current)
{
    return detail::from_ast_impl(t, detail::node_to_type_translator<Analyzer>{current});
}

template<class Analyzer>
helper::probable<any_type> from_ast(ast::node::any_type const& t, scope::any_scope const& current, Analyzer &a)
{
    return detail::from_ast_impl(t, detail::node_to_type_translator<Analyzer>{current, a});
}

} // namespace type
} // namespace dachs

#endif    // DACHS_SEMANTICS_TYPE_FROM_AST_HPP_INCLUDED
