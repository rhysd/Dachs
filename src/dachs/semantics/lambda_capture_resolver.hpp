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
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/helper/make.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;

struct is_captured : boost::static_visitor<bool> {
    symbol::var_symbol const& query;
    scope::func_scope const& threshold;

    template<class S>
    is_captured(decltype(query) const& q, S const& s)
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

    template<class Symbol>
    bool is_captured_symbol(Symbol const& sym) const
    {
        return boost::apply_visitor(is_captured{sym, lambda_scope}, current_scope);
    }

    std::string get_member_name() const noexcept
    {
        return receiver_symbol->name + ".capture." + std::to_string(offset);
    }

    ast::node::ufcs_invocation generate_invocation_from(ast::node::var_ref const& var)
    {
        auto const new_receiver_ref = helper::make<ast::node::var_ref>(receiver_symbol->name);
        new_receiver_ref->is_lhs_of_assignment = var->is_lhs_of_assignment;
        new_receiver_ref->symbol = receiver_symbol;
        new_receiver_ref->set_source_location(*var);
        // Note:
        // 'type' member will be set after the type of lambda object is determined.

        auto const new_invocation = helper::make<ast::node::ufcs_invocation>(new_receiver_ref, get_member_name());
        new_invocation->set_source_location(*var);
        new_invocation->type = var->type;

        auto const result = captures.insert({new_invocation, offset, var->symbol});
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

    explicit lambda_capture_resolver(scope::func_scope const& s, symbol::var_symbol const& r) noexcept
        : captures(), lambda_scope(s), offset(0u), current_scope(s), receiver_symbol(r)
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
    void visit(ast::node::statement_block &b, Walker const& w)
    {
        with_scope(b->scope, w);
    }

    template<class Walker>
    void visit(ast::node::let_stmt &let, Walker const& w)
    {
        with_scope(let->scope, w);
    }

    template<class Walker>
    void visit(ast::node::any_expr &e, Walker const& w)
    {
        if (auto const maybe_var = get_as<ast::node::var_ref>(e)) {
            auto const& var = *maybe_var;
            auto const symbol = var->symbol.lock();

            if (symbol->is_builtin) {
                return;
            }

            // TODO:
            // Deal with an ignoread variable, "_"

            {
                // Note:
                // 1 ufcs_invocation instance per 1 captured symbol.
                auto const already_replaced = sym_map.find(symbol);
                if (already_replaced != std::end(sym_map)) {
                    e = already_replaced->second;
                    return;
                }
            }

            auto const invocation = generate_invocation_from(var);
            // Note:
            // Replace var_ref with ufcs_invocation to access the member of lambda object.
            // The offset of member is memorized in lambda_capture map.
            e = invocation;
            sym_map[symbol] = invocation;

            return;
        }

        w();
    }

    template<class Node, class Walker>
    void visit(Node &, Walker const& w)
    {
        w();
    }
};

template<class Node>
captured_offset_map resolve_lambda_captures(Node &search_root, scope::func_scope const& lambda_scope, symbol::var_symbol const& receiver)
{
    lambda_capture_resolver resolver{lambda_scope, receiver};
    ast::walk_topdown(search_root, resolver);
    return resolver.get_captures();
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
