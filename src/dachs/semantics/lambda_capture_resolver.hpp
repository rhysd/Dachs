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
        for (auto const& m : clazz->instance_var_symbols) {
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
        new_receiver_ref->type = receiver_symbol->type;
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

        assert(!lambda->type || type::is_a<type::generic_func_type>(lambda->type));
        ast::walk_topdown(lambda->receiver, *this);
    }

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& w)
    {
        auto const symbol = get_symbol_from_var(var);
        if (!symbol) {
            return;
        }

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
captured_offset_map resolve_lambda_captures(Node &search_root, scope::func_scope const& lambda_scope, symbol::var_symbol const& receiver)
{
    lambda_capture_resolver resolver{lambda_scope, receiver};
    ast::walk_topdown(search_root, resolver);
    return resolver.get_captures();
}

class lambda_capture_replacer {
    captured_offset_map const& captures;

public:

    explicit lambda_capture_replacer(decltype(captures) const& cs)
        : captures(cs)
    {}

    template<class Walker>
    void visit(ast::node::lambda_expr const& lambda, Walker const&)
    {
        ast::walk_topdown(lambda->receiver, *this);
    }

    template<class Walker>
    void visit(ast::node::any_expr &e, Walker const& w)
    {
        auto const maybe_var_ref = get_as<ast::node::var_ref>(e);
        if (!maybe_var_ref) {
            w();
            return;
        }

        auto const& symbol = (*maybe_var_ref)->symbol.lock();
        auto const& indexed = captures.get<tags::refered_symbol>();
        auto const capture = indexed.find(symbol);
        if (capture != boost::end(indexed)) {
            e = capture->introduced;
        }
    }

    template<class Node, class Walker>
    void visit(Node const&, Walker const& w)
    {
        w();
    }
};

// Note:
// This function assumes that 'search_root' and its children are already analyzed.
// All symbols and types should be resolved normally.
template<class Node>
void resolve_lambda_capture_access(Node &search_root, captured_offset_map const& captures)
{
    lambda_capture_replacer replacer{captures};
    ast::walk_topdown(search_root, replacer);
}

class lambda_resolver {
    lambda_captures_type captures;

    template<class Node>
    void walk_recursively(Node &n)
    {
        ast::walk_topdown(n, *this);
    }

    symbol::var_symbol resolve_lambda_capture_map(ast::node::function_definition &func_def, type::generic_func_type const& lambda_type)
    {
        assert(!func_def->scope.expired());
        auto const func_scope = func_def->scope.lock();

        // Note:
        //  1. Lambda function takes its lambda object (captured values) as 1st parameter
        //  2. Analyze captures for the lambda function and return it to register

        auto const lambda_object_sym = symbol::make<symbol::var_symbol>(nullptr, "lambda.receiver", /*immutable*/true /*TODO*/);
        lambda_object_sym->type = lambda_type;

        auto const capture_map = resolve_lambda_captures(func_def, func_scope, lambda_object_sym);

        captures[lambda_type] = capture_map;

        return lambda_object_sym;
    }

    void set_lambda_receiver(type::generic_func_type const& lambda_type, ast::node::lambda_expr const& lambda)
    {
        // XXX:
        // Function invocation with do-end block (= predicate with lambda) should have a lambda object
        // on the 1st argument of the invocation.  The lambda object is created at the invocation and
        // it has captures for the do-end block.  However, currently Dachs doesn't have an AST node for
        // to generate a lambda object.  It should be done with class object construct.  Lambda object should
        // be an anonymous class object.
        // I'll implement the generation of lambda object with anonymous struct.  It must be replaced
        // with class object construction.

        assert(lambda->receiver->element_exprs.empty());
        assert(type::is_a<type::tuple_type>(lambda->receiver->type));

        auto const capture = captures.find(lambda_type);

        if (capture == std::end(captures)) {
            return;
        }

        auto const receiver_type = *type::get<type::tuple_type>(lambda->receiver->type);

        // Note:
        // Substitute captured values as its fields
        for (auto const& c : capture->second.template get<semantics::tags::offset>()) {
            auto const& s = c.refered_symbol;
            auto const new_var_ref = helper::make<ast::node::var_ref>(s->name);
            new_var_ref->symbol = c.refered_symbol;
            new_var_ref->type = s->type;
            new_var_ref->set_source_location(*lambda);
            lambda->receiver->element_exprs.push_back(new_var_ref);
            receiver_type->element_types.push_back(s->type);
        }
    }

    void set_lambda_receiver(ast::node::function_definition const& func_def, symbol::var_symbol const& receiver_sym)
    {
        auto const scope = func_def->scope.lock();
        auto const new_param = helper::make<ast::node::parameter>(!receiver_sym->immutable, receiver_sym->name, boost::none);

        new_param->set_source_location(*func_def);
        new_param->param_symbol = receiver_sym;
        new_param->type = receiver_sym->type;
        func_def->params.insert(std::begin(func_def->params), new_param);
        scope->force_push_front_param(receiver_sym);
    }

    template<class LambdaDef, class LambdaType>
    void resolve_lambda(LambdaDef &l, LambdaType const& t)
    {
        walk_recursively(l);

        auto const receiver = resolve_lambda_capture_map(l, t);
        set_lambda_receiver(l, receiver);
        resolve_lambda_capture_access(l, captures.at(t));
    }

public:

    lambda_captures_type get_captures() const
    {
        return captures;
    }

    template<class Walker>
    void visit(ast::node::lambda_expr &lambda, Walker const&)
    {
        auto &def = lambda->def;
        assert(type::is_a<type::generic_func_type>(lambda->type));
        auto const type = *type::get<type::generic_func_type>(lambda->type);

        if (helper::exists(captures, type)) {
            // Note:
            // Already resolved because of other lambda's dependency.
            return;
        }

        if (def->is_template()) {
            for (auto &i : def->instantiated) {
                resolve_lambda(i, type);
            }
        } else {
            resolve_lambda(def, type);
        }

        set_lambda_receiver(type, lambda);
    }

    template<class Node, class Walker>
    void visit(Node &, Walker const& w)
    {
        w();
    }
};


} // namespace detail
} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_LAMBDA_CAPTURE_RESOLVER_HPP_INCLUDED
