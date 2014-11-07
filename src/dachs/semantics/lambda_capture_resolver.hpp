#if !defined DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
#define      DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <unordered_map>
#include <string>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/fatal.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;

struct capture_checker : boost::static_visitor<bool> {
    symbol::var_symbol const& query;
    scope::func_scope const& threshold;

    template<class S>
    capture_checker(decltype(query) const& q, S const& s)
        : query(q), threshold(s)
    {}

    bool operator()(scope::weak_global_scope const&) const
    {
        // Not reached here because the symbol is already resolved
        // and the threshold scope must be in the looking path.
        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
    }

    bool operator()(scope::weak_local_scope const& local_) const
    {
        auto const local = local_.lock();
        for (auto const& v : local->local_vars) {
            if (*query == *v) {
                return false;
            }
        }

        return boost::apply_visitor(*this, local->enclosing_scope);
    }

    bool operator()(scope::weak_func_scope const& func_) const
    {
        auto const func = func_.lock();
        for (auto const& p : func->params) {
            if (*query == *p) {
                return false;
            }
        }

        if (*threshold == *func) {
            return true;
        }

        return boost::apply_visitor(*this, func->enclosing_scope);
    }

    bool operator()(scope::weak_class_scope const& clazz_) const
    {
        auto const clazz = clazz_.lock();
        for (auto const& m : clazz->member_var_symbols) {
            if (*query == *m) {
                return false;
            }
        }

        return boost::apply_visitor(*this, clazz->enclosing_scope);
    }
};

class lambda_capture_resolver {
    captured_offset_map captures;
    scope::func_scope const& lambda_scope;
    size_t offset;
    scope::any_scope current_scope;
    symbol::var_symbol const& receiver_symbol;
    std::unordered_map<symbol::var_symbol, ast::node::ufcs_invocation> sym_map;
    std::unordered_map<type::generic_func_type, ast::node::tuple_literal> &lambda_instantiations;
    std::unordered_map<scope::func_scope, symbol::var_symbol> lambda_object_symbol_map;

    template<class Symbol>
    bool check_captured_symbol(Symbol const& sym) const
    {
        return boost::apply_visitor(capture_checker{sym, lambda_scope}, current_scope);
    }

    std::string get_member_name() const noexcept
    {
        return receiver_symbol->name + ".capture." + std::to_string(offset);
    }

    ast::node::ufcs_invocation generate_invocation_from(ast::node::var_ref const& var, symbol::var_symbol const& symbol)
    {
        auto const new_receiver_ref = helper::make<ast::node::var_ref>(receiver_symbol->name);
        new_receiver_ref->is_lhs_of_assignment = var->is_lhs_of_assignment;
        new_receiver_ref->symbol = receiver_symbol;
        new_receiver_ref->set_source_location(*var);
        // Note:
        // 'type' member will be set after the type of lambda object is determined.

        auto const new_invocation = helper::make<ast::node::ufcs_invocation>(new_receiver_ref, get_member_name());
        new_invocation->set_source_location(*var);
        new_invocation->type = symbol->type;

        auto const result = captures.insert({new_invocation, offset, symbol});
        assert(result.second);
        (void) result;

        ++offset;
        return new_invocation;
    }

    template<class S, class W>
    void with_scope(std::weak_ptr<S> const& ws, W const& w)
    {
        auto const tmp = current_scope;
        current_scope = ws.lock();
        w();
        current_scope = tmp;
    }

    template<class S, class W>
    void with_scope(std::shared_ptr<S> const& ss, W const& w)
    {
        auto const tmp = current_scope;
        current_scope = ss;
        w();
        current_scope = tmp;
    }

public:

    explicit lambda_capture_resolver(scope::func_scope const& s, symbol::var_symbol const& r, decltype(lambda_instantiations) &i) noexcept
        : captures(), lambda_scope(s), offset(0u), current_scope(s), receiver_symbol(r), lambda_instantiations(i)
    {}

    template<class Scope>
    lambda_capture_resolver(scope::func_scope const& s, Scope const& current, symbol::var_symbol const& r) noexcept
        : captures(), lambda_scope(s), offset(0u), current_scope(current), receiver_symbol(r)
    {}

    auto get_captures() const
    {
        return captures;
    }

    template<class Walker>
    void visit(ast::node::statement_block const& b, Walker const& w)
    {
        with_scope(b->scope, w);
    }

    template<class Walker>
    void visit(ast::node::let_stmt const& let, Walker const& w)
    {
        with_scope(let->scope, w);
    }

    template<class Walker>
    void visit(ast::node::lambda_expr const& lambda, Walker const&)
    {
        // Note:
        // Update symbols in lambda object instantiation

        assert(type::is_a<type::generic_func_type>(lambda->type));
        auto const instantiation = lambda_instantiations.find(*type::get<type::generic_func_type>(lambda->type));
        if (instantiation != std::end(lambda_instantiations)) {
            ast::walk_topdown(instantiation->second, *this);
        }
    }

    symbol::var_symbol get_symbol_from_var(ast::node::var_ref const& var)
    {
        if (var->symbol.expired()) {
            // Note:
            // Fallback.  When the target is a function template,
            // it is never analyzed by symbol_analyzer and the
            // variable symbol is expired.

            auto name = var->name;
            if (name.back() == '!') {
                name.pop_back();
            }

            auto const maybe_var_symbol = boost::apply_visitor(scope::var_symbol_resolver{name}, current_scope);
            if (!maybe_var_symbol) {
                // Note:
                // It must be a semantic error.
                return nullptr;
            }

            var->type = (*maybe_var_symbol)->type;

            return *maybe_var_symbol;
        }

        return var->symbol.lock();
    }

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& w)
    {
        auto const symbol = get_symbol_from_var(var);

        if (symbol->is_builtin ||
                symbol->is_global ||
                !check_captured_symbol(symbol) ||
                helper::exists(sym_map, symbol)) {
            // Note:
            // 1 ufcs_invocation instance per 1 captured symbol.
            return;
        }

        sym_map[symbol] = generate_invocation_from(var, symbol);

        w();
    }

    template<class Node, class Walker>
    void visit(Node const&, Walker const& w)
    {
        w();
    }
};

template<class Node>
captured_offset_map resolve_lambda_captures(Node &search_root, scope::func_scope const& lambda_scope, symbol::var_symbol const& receiver, std::unordered_map<type::generic_func_type, ast::node::tuple_literal> &lambda_instantiations)
{
    lambda_capture_resolver resolver{lambda_scope, receiver, lambda_instantiations};
    ast::walk_topdown(search_root, resolver);
    return resolver.get_captures();
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
