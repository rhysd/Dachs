#include <cassert>
#include <cstddef>

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

using std::size_t;
using dachs::helper::variant::get;
using dachs::helper::variant::has;

struct type_of_visitor
    : public boost::static_visitor<type::type> {

    template<class T>
    type::type operator()(std::shared_ptr<T> const& node) const noexcept
    {
        return node->type;
    }
};

template<class Variant>
inline type::type type_of(Variant const& v) noexcept
{
    return boost::apply_visitor(type_of_visitor{}, v);
}

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

    size_t failed;

    template<class Scope>
    explicit forward_symbol_analyzer(Scope const& s) noexcept
        : current_scope(s), failed(0)
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
        auto new_func = make<func_scope>(func_def, global_scope, func_def->name);
        func_def->scope = new_func;
        if (!global_scope->define_function(new_func)) {
            failed++;
        } else {
            // If the symbol passes duplication check, it also defines as variable
            auto new_func_var = symbol::make<symbol::var_symbol>(func_def, func_def->name);
            global_scope->define_global_constant(new_func_var);
        }
        with_new_scope(std::move(new_func), recursive_walker);
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

    size_t failed;

    template<class Scope>
    explicit symbol_analyzer(Scope const& root, global_scope const& global) noexcept
        : current_scope{root}, global{global}, failed{0}
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
    // }}}

    // Do not analyze in forward_symbol_analyzer because variables can't be referenced forward {{{
    template<class Walker>
    void visit(ast::node::constant_decl const& const_decl, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_var = symbol::make<symbol::var_symbol>(const_decl, const_decl->name);
        const_decl->symbol = new_var;
        if (global_scope->define_global_constant(new_var)) {
            failed++;
        }
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& recursive_walker)
    {
        auto new_param = symbol::make<symbol::var_symbol>(param, param->name);
        param->param_symbol = new_param;
        if (auto maybe_func = get<func_scope>(current_scope)) {
            auto& func = *maybe_func;
            if (!func->define_param(new_param)) {
                failed++;
            }

            if (!param->param_type) {
                // Type is not specified. Register template parameter.
                auto const tmpl = dachs::symbol::make<dachs::symbol::template_type_symbol>(new_param->name);
                func->define_template_param(tmpl);
                param->template_type_ref = tmpl;
            }

        } else if (auto maybe_local = get<local_scope>(current_scope)) {
            // When for statement
            auto& local = *maybe_local;
            if (!local->define_local_var(new_param)) {
                failed++;
            }
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
        auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name);
        decl->symbol = new_var;
        if (!local->define_local_var(new_var)) {
            failed++;
        }
        recursive_walker();
    }
    // }}}

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& recursive_walker)
    {
        auto maybe_resolved_symbol = boost::apply_visitor(var_symbol_resolver_for_shared_scope{var->name}, current_scope);
        if (maybe_resolved_symbol) {
            var->symbol = *maybe_resolved_symbol;
        } else {
            std::cerr << "Semantic error at line:" << var->line << ", col:" << var->col << "\nSymbol '" << var->name << "' is not found." << std::endl;
            failed++;
        }
        recursive_walker();
    }


    // Get built-in data types {{{
    template<class Walker>
    void visit(ast::node::primary_literal const& primary_lit, Walker const& /*unused because it doesn't has child*/)
    {
        struct builtin_type_name_selector
            : public boost::static_visitor<char const* const> {

            result_type operator()(char const) const noexcept
            {
                return "char";
            }

            result_type operator()(double const) const noexcept
            {
                return "float";
            }

            result_type operator()(bool const) const noexcept
            {
                return "bool";
            }

            result_type operator()(std::string const&) const noexcept
            {
                return "string";
            }

            result_type operator()(int const) const noexcept
            {
                return "int";
            }

            result_type operator()(unsigned int const) const noexcept
            {
                return "uint";
            }
        } selector;

        primary_lit->type = type::get_builtin_type(boost::apply_visitor(selector, primary_lit->value));
    }

    template<class Walker>
    void visit(ast::node::symbol_literal const& sym_lit, Walker const& /*unused because it doesn't has child*/)
    {
        sym_lit->type = type::get_builtin_type("symbol");
    }

    template<class Walker>
    void visit(ast::node::array_literal const& arr_lit, Walker const& recursive_walker)
    {
        recursive_walker();
        // Note: Check only the head of element because Dachs doesn't allow implicit type conversion
        arr_lit->type = type::make<type::array_type>(type_of(arr_lit->element_exprs[0]));
    }

    template<class Walker>
    void visit(ast::node::tuple_literal const& tuple_lit, Walker const& recursive_walker)
    {
        recursive_walker();
        auto const type = type::make<type::tuple_type>();
        type->element_types.reserve(tuple_lit->element_exprs.size());
        for (auto const& e : tuple_lit->element_exprs) {
            type->element_types.push_back(type_of(e));
        }
        tuple_lit->type = type;
    }

    template<class Walker>
    void visit(ast::node::dict_literal const& dict_lit, Walker const& recursive_walker)
    {
        recursive_walker();
        // Note: Check only the head of element because Dachs doesn't allow implicit type conversion
        auto const& p = dict_lit->value[0];
        dict_lit->type = type::make<type::dict_type>(type_of(p.first), type_of(p.second));
    }

    template<class Walker>
    void visit(ast::node::binary_expr const& bin_expr, Walker const& recursive_walker)
    {
        recursive_walker();
        if (bin_expr->op == ".." || bin_expr->op == "...") {
            bin_expr->type = type::make<type::range_type>(type_of(bin_expr->lhs), type_of(bin_expr->rhs));
        }
    }
    // }}}

    template<class Walker>
    void visit(ast::node::member_access const& /*member_access*/, Walker const& /*unused*/)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "member access"};
    }

    template<class Walker>
    void visit(ast::node::object_construct const& /*member_access*/, Walker const& /*unused*/)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "object construction"};
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
        // Builtin functions

        // func print(str)
        auto print_func = make<func_scope>(a.root, tree_root, "print");
        print_func->body = make<local_scope>(print_func);
        // Note: These definitions are never duplicate
        print_func->define_param(symbol::make<symbol::var_symbol>(a.root, "str"));
        tree_root->define_function(print_func);
        tree_root->define_global_constant(symbol::make<symbol::var_symbol>(a.root, "print"));
    }

    {
        detail::forward_symbol_analyzer forward_resolver{tree_root};
        ast::walk_topdown(a.root, forward_resolver);

        if (forward_resolver.failed > 0) {
            throw dachs::semantic_check_error{forward_resolver.failed, "forward symbol resolution"};
        }
    }

    {
        detail::symbol_analyzer resolver{tree_root, tree_root};
        ast::walk_topdown(a.root, resolver);

        if (resolver.failed > 0) {
            throw dachs::semantic_check_error{resolver.failed, "symbol resolution"};
        }
    }

    // TODO: get type of global function variables' type on visit node::function_definition and node::procedure_definition

    return scope_tree{tree_root};
}

} // namespace scope
} // namespace dachs
