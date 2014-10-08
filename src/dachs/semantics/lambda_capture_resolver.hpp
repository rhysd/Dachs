#if !defined DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
#define      DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED

#include <cstddef>
#include <memory>
#include <unordered_map>

#include "dachs/ast/ast.hpp"
#include "dachs/symbol.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/semantics_context.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;

class lambda_capture_resolver {
    captured_offset_map captures;
    scope::func_scope const& lambda_scope;
    size_t offset;
    scope::any_scope current_scope;
    std::unordered_map<symbol::var_symbol, symbol::var_symbol> sym_map;

    template<class Symbol>
    bool is_captured_symbol(Symbol const& sym) const
    {
        return false;
    }

    template<class Symbol>
    void add_capture(Symbol const& new_sym)
    {
        captures[new_sym] = offset;
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

    auto get_captures() const
    {
        return captures;
    }

    template<class Walker>
    void visit(ast::node::var_ref const& ref, Walker const& w)
    {
        if (is_captured_symbol(ref->symbol)) {
            // TODO:
            // Make new symbol for capture
            // register it to captures
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

    template<class Walker>
    void visit(ast::node::do_stmt const& do_, Walker const& w)
    {
        with_scope(do_->scope, w);
    }

    template<class Node, class Walker>
    void visit(Node const& n, Walkker const& w)
    {
        w();
    }
};

} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
