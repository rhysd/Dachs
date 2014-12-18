#if !defined DACHS_SEMANTICS_ANALYZER_COMMON_HPP_INCLUDED
#define      DACHS_SEMANTICS_ANALYZER_COMMON_HPP_INCLUDED

#include <vector>
#include <cassert>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/fatal.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using type::type_of;

struct class_resolver
    : boost::static_visitor<boost::optional<scope::class_scope>> {
    std::string const& name;

    explicit class_resolver(std::string const& n) noexcept
        : name{n}
    {}

    template<class T>
    result_type operator()(std::shared_ptr<T> const& scope) const noexcept
    {
        return scope->resolve_class(name);
    }
};

class type_calculator_from_type_nodes
    : public boost::static_visitor<type::type> {

    scope::any_scope const& current_scope;

    template<class T>
    type::type apply_recursively(T const& t) const noexcept
    {
        return boost::apply_visitor(*this, t);
    }

public:

    explicit type_calculator_from_type_nodes(scope::any_scope const& c)
        : current_scope(c)
    {}

    type::type operator()(ast::node::primary_type const& t) const noexcept
    {
        auto const builtin = type::get_builtin_type(t->template_name.c_str());
        if (builtin) {
            return *builtin;
        } else {
            auto const c = boost::apply_visitor(class_resolver{t->template_name}, current_scope);
            if (!c) {
                return {};
            }

            auto const ret = type::make<type::class_type>(t->template_name, *c);
            for (auto const& instantiated : t->instantiated_templates) {
                ret->template_types.push_back(apply_recursively(instantiated));
            }
            return ret;
        }
    }

    type::type operator()(ast::node::array_type const& t) const noexcept
    {
        return type::make<type::array_type>(
                    apply_recursively(t->elem_type)
                );
    }

    type::type operator()(ast::node::tuple_type const& t) const noexcept
    {
        auto const ret = type::make<type::tuple_type>();
        ret->element_types.reserve(t->arg_types.size());
        for (auto const& arg : t->arg_types) {
            ret->element_types.push_back(apply_recursively(arg));
        }
        return ret;
    }

    type::type operator()(ast::node::dict_type const& t) const noexcept
    {
        return type::make<type::dict_type>(
                    apply_recursively(t->key_type),
                    apply_recursively(t->value_type)
                );
    }

    type::type operator()(ast::node::qualified_type const& t) const noexcept
    {
        type::qualifier new_qualifier;
        switch (t->qualifier) {
        case ast::symbol::qualifier::maybe:
            new_qualifier = type::qualifier::maybe;
            break;
        default:
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        return type::make<type::qualified_type>(
                    new_qualifier, apply_recursively(t->type)
               );
    }

    type::type operator()(ast::node::func_type const& t) const noexcept
    {
        std::vector<type::any_type> param_types;
        param_types.reserve(t->arg_types.size());
        for (auto const& a : t->arg_types) {
            param_types.push_back(apply_recursively(a));
        }

        if (t->ret_type) {
            return {type::make<type::func_type>(
                    std::move(param_types),
                    apply_recursively(*(t->ret_type))
                )};
        } else {
            return {type::make<type::proc_type>(std::move(param_types))};
        }
    }
};

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_ANALYZER_COMMON_HPP_INCLUDED
