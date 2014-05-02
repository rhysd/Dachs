#include <cassert>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/range/algorithm/find_if.hpp>

#include "dachs/scope.hpp"
#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"
#include "dachs/ast_walker.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace scope {

namespace detail {

using dachs::helper::variant::get;
using dachs::helper::variant::has;

// Walk to analyze functions, classes and member variables symbols to make forward reference possible
class forward_symbol_analyzer {

    any_scope current_scope;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker>
    void with_new_scope(Scope && new_scope, Walker const& walker)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        walker();
        current_scope = tmp_scope;
    }

public:

    bool failed;

    template<class Scope>
    explicit forward_symbol_analyzer(Scope const& s) noexcept
        : current_scope(s), failed(false)
    {}

    template<class Walker>
    void visit(ast::node::statement_block const& block, Walker const& recursive_walker)
    {
        auto new_local_scope = make<local_scope>(current_scope);
        block->scope = new_local_scope;
        if (auto maybe_local_scope = get<local_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_local_scope;
            enclosing_scope->define_child(new_local_scope);
        } else if (auto maybe_func_scope = get<func_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_func_scope;
            enclosing_scope->body = new_local_scope;
        } else {
            assert(false); // Should not reach here
        }
        with_new_scope(std::move(new_local_scope), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_func = make<func_scope>(global_scope, func_def->name->value);
        func_def->scope = new_func;
        global_scope->define_function(new_func);
        auto new_func_var = symbol::make<symbol::var_symbol>(func_def->name->value);
        global_scope->define_global_constant(new_func_var);
        with_new_scope(std::move(new_func), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::procedure_definition const& proc_def, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_proc = make<func_scope>(global_scope, proc_def->name->value);
        proc_def->scope = new_proc;
        global_scope->define_function(new_proc);
        auto new_proc_var = symbol::make<symbol::var_symbol>(proc_def->name->value);
        global_scope->define_global_constant(new_proc_var);
        with_new_scope(std::move(new_proc), recursive_walker);
    }

    // TODO: class scopes and member function scopes

    template<class T, class Walker>
    void visit(T const&, Walker const& walker)
    {
        // Simply visit children recursively
        walker();
    }
};

struct weak_ptr_locker : public boost::static_visitor<any_scope> {
    template<class WeakScope>
    any_scope operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock();
    }
};

struct var_symbol_resolver_for_shared_scope
    : boost::static_visitor<boost::optional<symbol::var_symbol>> {
    std::string const& name;

    explicit var_symbol_resolver_for_shared_scope(std::string const& n) noexcept
        : name{n}
    {}

    template<class T>
    boost::optional<symbol::var_symbol> operator()(std::shared_ptr<T> const& scope) const noexcept
    {
        return scope->resolve_var(name);
    }
};

// Walk to resolve symbol references
class symbol_analyzer {

    any_scope current_scope;
    global_scope const global;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker>
    void with_new_scope(Scope const& new_scope, Walker const& walker)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        walker();
        current_scope = tmp_scope;
    }

public:

    bool failed;

    template<class Scope>
    explicit symbol_analyzer(Scope const& root, global_scope const& global) noexcept
        : current_scope{root}, global{global}, failed{false}
    {}

    // Push and pop current scope {{{
    template<class Walker>
    void visit(ast::node::statement_block const& block, Walker const& recursive_walker)
    {
        assert(!block->scope.expired());
        with_new_scope(block->scope.lock(), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func, Walker const& recursive_walker)
    {
        assert(!func->scope.expired());
        with_new_scope(func->scope.lock(), recursive_walker);
    }

    template<class Walker>
    void visit(ast::node::procedure_definition const& proc, Walker const& recursive_walker)
    {
        assert(!proc->scope.expired());
        with_new_scope(proc->scope.lock(), recursive_walker);
    }
    // }}}

    // Do not analyze in forward_symbol_analyzer because variables can't be referenced forward {{{
    template<class Walker>
    void visit(ast::node::constant_decl const& const_decl, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_var = symbol::make<symbol::var_symbol>(const_decl, const_decl->name->value);
        const_decl->symbol = new_var;
        global_scope->define_global_constant(new_var);
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& recursive_walker)
    {
        auto new_param = symbol::make<symbol::var_symbol>(param, param->name->value);
        param->param_symbol = new_param;
        if (auto maybe_func = get<func_scope>(current_scope)) {
            auto& func = *maybe_func;
            func->define_param(new_param);

            if (!param->param_type) {
                // Type is not specified. Register template parameter.
                auto const tmpl = dachs::symbol::make<dachs::symbol::template_type_symbol>(new_param->name);
                func->define_template_param(tmpl);
                param->template_type_ref = tmpl;
            }

        } else if (auto maybe_local = get<local_scope>(current_scope)) {
            // When for statement
            auto& local = *maybe_local;
            local->define_local_var(new_param);
        } else {
            assert(false);
        }
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& recursive_walker)
    {
        auto maybe_local = get<local_scope>(current_scope);
        assert(maybe_local);
        auto& local = *maybe_local;
        auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name->value);
        decl->symbol = new_var;
        local->define_local_var(new_var);
        recursive_walker();
    }
    // }}}

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& recursive_walker)
    {
        auto maybe_resolved_symbol = boost::apply_visitor(var_symbol_resolver_for_shared_scope{var->name->value}, current_scope);
        if (maybe_resolved_symbol) {
            var->symbol = *maybe_resolved_symbol;
        } else {
            std::cerr << "Semantic error at line:" << var->line << ", col:" << var->col << "\nSymbol '" << var->name->value << "' is not found." << std::endl;
            failed = true;
        }
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::member_access const& /*member_access*/, Walker const& /*unused*/)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "member access"};
    }

    // TODO: member variable accesses
    // TODO: method accesses

    template<class T, class Walker>
    void visit(T const&, Walker const& walker)
    {
        // simply visit children recursively
        walker();
    }
};

} // namespace detail

scope_tree make_scope_tree(ast::ast &a)
{
    auto const tree_root = make<global_scope>();

    {
        detail::forward_symbol_analyzer forward_resolver{tree_root};
        ast::walk_topdown(a.root, forward_resolver);

        if (forward_resolver.failed) {
            throw dachs::semantic_check_error{"forward symbol resolution"};
        }
    }

    {
        detail::symbol_analyzer resolver{tree_root, tree_root};
        ast::walk_topdown(a.root, resolver);

        if (resolver.failed) {
            throw dachs::semantic_check_error{"symbol resolution"};
        }
    }

    // TODO: get type of global function variables' type on visit node::function_definition and node::procedure_definition

    return scope_tree{tree_root};
}

} // namespace scope
} // namespace dachs
