#if !defined DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
#define      DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <unordered_map>

#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/fatal.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantics_context.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;

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

    using replaced_symbol = symbol::var_symbol;
    using captured_symbol = symbol::var_symbol;
    std::unordered_map<captured_symbol, replaced_symbol> sym_map;

    template<class Symbol>
    bool is_captured_symbol(Symbol const& sym) const
    {
        return boost::apply_visitor(is_captured{sym, lambda_scope}, current_scope);
    }

    template<class Symbol>
    void add_capture(Symbol const& new_sym)
    {
        captures.insert({new_sym, offset});
        ++offset;
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

    explicit lambda_capture_resolver(scope::func_scope const& s) noexcept
        : captures(), lambda_scope(s), offset(0u), current_scope(s)
    {}

    template<class Scope>
    lambda_capture_resolver(scope::func_scope const& s, Scope const& current) noexcept
        : captures(), lambda_scope(s), offset(0u), current_scope(current)
    {}

    auto get_captures() const
    {
        return captures;
    }

    template<class Walker>
    void visit(ast::node::var_ref const& ref, Walker const& w)
    {
        auto const sym = ref->symbol.lock();

        if (sym->is_builtin) {
            return;
        }

        // TODO:
        // Check the variable is defined in global scope.
        // If then, global variables should not be captured.

        {
            auto const already_replaced = sym_map.find(sym);
            if (already_replaced != std::end(sym_map)) {
                ref->symbol = already_replaced->second;
                return;
            }
        }

        if (is_captured_symbol(sym)) {
            auto const replaced = symbol::make<symbol::var_symbol>(*sym);
            add_capture(replaced);
            sym_map[sym] = replaced;
            ref->symbol = replaced;
        }

        w();
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

    template<class Node, class Walker>
    void visit(Node const&, Walker const& w)
    {
        w();
    }
};

template<class Node>
captured_offset_map resolve_lambda_captures(Node &search_root, scope::func_scope const& lambda_scope)
{
    lambda_capture_resolver resolver{lambda_scope};
    ast::walk_topdown(search_root, resolver);
    return resolver.get_captures();
}

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
