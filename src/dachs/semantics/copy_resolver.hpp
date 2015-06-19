#if !defined DACHS_SEMANTICS_COPY_RESOLVER_HPP_INCLUDED
#define      DACHS_SEMANTICS_COPY_RESOLVER_HPP_INCLUDED

#include <boost/variant/static_visitor.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace semantics {
namespace detail {

template<class Analyzer, class Node>
class copy_resolver : public boost::static_visitor<bool> {
    Analyzer &a;
    Node const& node;

    bool apply(type::type const& t)
    {
        return t.apply_visitor(*this);
    }

public:

    copy_resolver(Analyzer &a, Node const& n) noexcept
        : a(a), node(n)
    {}

    bool operator()(type::tuple_type const& t)
    {
        for (auto const& e : t->element_types) {
            if (!apply(e)) {
                return false;
            }
        }
        return true;
    }

    bool operator()(type::array_type const& t)
    {
        return apply(t->element_type);
    }

    bool operator()(type::pointer_type const& t)
    {
        return apply(t->pointee_type);
    }

    bool operator()(type::qualified_type const& t)
    {
        return apply(t->contained_type);
    }

    bool operator()(type::class_type const& t)
    {
        if (a.copiers.find(t) != std::end(a.copiers)) {
            // Note: Already resolved
            return true;
        }

        type::type wrapped{t};

        auto const candidates =
            a.with_current_scope(
                    [&](auto const& s)
                    {
                        return s->resolve_func("dachs.copy", {wrapped});
                    }
                );

        if (candidates.empty()) {
            return true;
        }

        if (candidates.size() > 1u) {
            std::string notes;
            for (auto const& c : candidates) {
                notes += "\n  Candidate: " + c->to_string();
            }
            a.semantic_error(node, "  Invalid copier for '" + t->to_string() + "'" + notes);
            return false;
        }

        auto func = std::move(*std::begin(candidates));
        auto func_def = func->get_ast_node();
        assert(!func->is_builtin);

        if (func->is_template()) {
            std::tie(func_def, func) = a.instantiate_function_from_template(func_def, func, {wrapped});
        }

        if (!func_def->ret_type) {
            if (!a.walk_recursively_with(a.global, func_def)) {
                a.semantic_error(
                        node,
                        "  Failed to analyze copier defined at " + func_def->location.to_string()
                    );
                return false;
            }
        }

        if (!func->ret_type) {
            a.semantic_error(
                    node,
                    "  Cannot deduce the return type of copier"
                );
            return false;
        }

        // Note:
        // Check function accessibility
        if (!func_def->is_public()) {
            auto const f = a.with_current_scope([](auto const& s)
                    {
                        return s->get_enclosing_func();
                    }
                );
            auto const cls = type::get<type::class_type>(func->params[0]->type);

            assert(f);
            assert(cls);

            auto const error
                = [&]
                {
                    a.semantic_error(
                            node,
                            boost::format("  member function '%1%' is a private member of class '%2%'")
                                % func->to_string() % (*cls)->name
                        );
                    return false;
                };

            if (auto const c = (*f)->get_receiver_class_scope()) {
                if ((*cls)->name != (*c)->name) {
                    return error();
                }
            } else {
                return error();
            }
        }

        a.copiers.emplace(t, func);

        assert(t->param_types.empty());
        for (auto const& i : t->ref.lock()->instance_var_symbols) {
            if (!apply(i->type)) {
                return false;
            }
        }

        return true;
    }

    template<class T>
    bool operator()(T const&)
    {
        return true;
    }
};

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_COPY_RESOLVER_HPP_INCLUDED
