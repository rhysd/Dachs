#if !defined DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
#define      DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED

#include <iterator>
#include <algorithm>
#include <cstddef>
#include <cassert>

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/semantics/forward_analyzer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/exception.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::has;
using helper::variant::apply_lambda;

// Walk to analyze functions, classes and member variables symbols to make forward reference possible
class forward_symbol_analyzer {

    scope::any_scope current_scope;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker>
    void introduce_scope_and_walk(Scope && new_scope, Walker const& walker)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        walker();
        current_scope = tmp_scope;
    }

    template<class Node, class Message>
    void semantic_error(Node const& n, Message const& msg) noexcept
    {
        output_semantic_error(n, msg);
        failed++;
    }

    std::string get_lambda_name(ast::node::lambda_expr const& l) const noexcept
    {
        return "lambda."
            + std::to_string(l->line)
            + '.' + std::to_string(l->col)
            + '.' + std::to_string(l->length)
            + '.' + helper::hex_string_of_ptr(l->def.get());
    }

    template<class Predicate>
    auto with_current_scope(Predicate const& p) const
    {
        return apply_lambda(
                    p,
                    current_scope
                );
    }

    template<class Funcs, class String>
    std::size_t check_functions_duplication(Funcs const& functions, String const& situation) const
    {
        std::size_t failed = 0u;
        auto const end = functions.cend();
        for (auto left = functions.cbegin(); left != end; ++left) {
            for (auto right = std::next(left); right != end; ++right) {
                if (**right == **left) {
                    auto const rhs_def = (*right)->get_ast_node();
                    auto const lhs_def = (*left)->get_ast_node();
                    output_semantic_error(
                            rhs_def,
                            boost::format(
                                "  In %1%, '%2%' is redefined.\n"
                                "  Note: Previous definition is at line:%3%, col:%4%"
                            ) % situation % (*right)->to_string() % lhs_def->line % lhs_def->col
                        );
                    failed++;
                }
            }
        }

        return failed;
    }

    template<class ClassDefs>
    std::size_t check_classes_duplication(ClassDefs const& classes) const
    {
        std::size_t failed = 0u;
        auto const end = classes.cend();
        for (auto left = classes.cbegin(); left != end; ++left) {
            for (auto right = std::next(left); right != end; ++right) {
                if ((*right)->name == (*left)->name) {
                    output_semantic_error(
                            *right,
                            boost::format(
                                "  Class '%1%' is redefined.\n"
                                "  Note: Previous definition is at line:%2%, col:%3%"
                            ) % (*right)->name % (*left)->line % (*left)->col
                        );
                    failed++;
                }
            }
        }

        return failed;
    }

    template<class Location>
    ast::node::var_ref generate_self_ref(Location const& location) const
    {
        auto const ref = helper::make<ast::node::var_ref>("self");
        ref->set_source_location(location);
        return ref;
    }

    ast::node::ufcs_invocation generate_self_member_access(ast::node::var_ref const& v) const
    {
        auto const self = generate_self_ref(*v);
        auto const access = helper::make<ast::node::ufcs_invocation>(self, v->name.substr(1u));
        access->set_source_location(*v);
        return access;
    }

    template<class String, class Location>
    ast::node::parameter generate_receiver_node(String const& class_name, Location const& location) const
    {
        auto const receiver_type_node = helper::make<ast::node::primary_type>(class_name);
        receiver_type_node->set_source_location(location);

        auto const receiver_node
            = helper::make<ast::node::parameter>(
                    true/* is_var */,
                    "self",
                    ast::node::any_type{receiver_type_node},
                    true/* is_receiver  */
                );
        receiver_node->set_source_location(location);

        receiver_node->param_type = receiver_type_node;
        return receiver_node;
    }

    void grow_default_ctor_ast(ast::node::class_definition const& def)
    {
        std::vector<ast::node::parameter> params
            = {
                generate_receiver_node(def->name, *def)
            };

        auto ctor_def
            = helper::make<ast::node::function_definition>(
                ast::node_type::function_definition::ctor_tag{},
                params,
                helper::make<ast::node::statement_block>()
            );
        ctor_def->set_source_location(*def);

        def->member_funcs.emplace_back(std::move(ctor_def));
    }

    void grow_memberwise_ctor_ast(ast::node::class_definition const& def)
    {
        if (def->instance_vars.empty()) {
            return;
        }

        std::vector<ast::node::parameter> params
            = {
                generate_receiver_node(def->name, *def)
            };

        for (auto const& d : def->instance_vars) {
            params.emplace_back(
                    helper::make<ast::node::parameter>(
                            true,
                            '@' + d->name,
                            boost::none
                        )
                );
            params.back()->set_source_location(*def);
        }

        auto ctor_def
            = helper::make<ast::node::function_definition>(
                ast::node_type::function_definition::ctor_tag{},
                params,
                helper::make<ast::node::statement_block>()
            );
        ctor_def->set_source_location(*def);

        def->member_funcs.emplace_back(std::move(ctor_def));
    }

public:

    size_t failed;

    template<class Scope>
    explicit forward_symbol_analyzer(Scope const& s) noexcept
        : current_scope(s), failed(0)
    {}

    template<class Walker>
    void visit(ast::node::inu const& inu, Walker const& w)
    {
        // Note:
        // Add receiver parameter to member functions' parameters here because this operation makes side effect to AST
        // and it causes a problem when re-visiting class_definition to instantiate class template
        // if this operation is done at visiting class_definition.
        for (auto const& c : inu->classes) {
            bool has_user_ctor = false;

            for (auto const& m : c->member_funcs) {
                m->params.insert(std::begin(m->params), generate_receiver_node(c->name, *m));
                if (m->is_ctor()) {
                    has_user_ctor = true;
                }
            }

            if (!has_user_ctor) {
                grow_default_ctor_ast(c);
                grow_memberwise_ctor_ast(c);
            }
        }

        // Note:
        // Visit classes at first because class definitions are needed class type
        // is specified at parsing parameter
        w(inu->classes, inu->functions, inu->global_constants);

        // Note:
        // Move all member functions to global.
        // (their function scopes are already defined in global scope at visiting their function_definition)
        for (auto const& c : inu->classes) {
            std::move(
                    std::begin(c->member_funcs),
                    std::end(c->member_funcs),
                    std::back_inserter(inu->functions)
                );
            c->member_funcs.clear();
        }

        auto const global = get_as<scope::global_scope>(current_scope);
        failed += check_functions_duplication((*global)->functions, "global scope");
        failed += check_classes_duplication(inu->classes);
    }

    template<class Walker>
    void visit(ast::node::statement_block const& block, Walker const& w)
    {
        auto const new_local_scope = scope::make<scope::local_scope>(current_scope);
        block->scope = new_local_scope;
        if (auto maybe_local_scope = get_as<scope::local_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_local_scope;
            enclosing_scope->define_child(new_local_scope);
        } else if (auto maybe_func_scope = get_as<scope::func_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_func_scope;
            enclosing_scope->body = new_local_scope;
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
        introduce_scope_and_walk(std::move(new_local_scope), w);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& w)
    {
        if (boost::algorithm::starts_with(func_def->name, "__builtin_")) {
            semantic_error(
                    func_def,
                    "  Only built-in functions' names are permitted to prefix '__builtin_'"
                );
            return;
        }

        // Define scope
        auto new_func = scope::make<scope::func_scope>(func_def, current_scope, func_def->name);
        new_func->type = type::make<type::generic_func_type>(scope::weak_func_scope{new_func});
        func_def->scope = new_func;

        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            semantic_error(func_def, boost::format("  Procedure '%1%' can't have return type") % func_def->name);
            return;
        }

        // Note:
        // Get return type for checking duplication of overloaded function
        if (func_def->return_type) {
            auto const ret_type = type::from_ast(*func_def->return_type, current_scope);
            func_def->ret_type = ret_type;
            new_func->ret_type = ret_type;
        }

        if (!func_def->params.empty() && func_def->params[0]->is_receiver) {
            new_func->is_member_func = true;
        }

        struct func_definer : boost::static_visitor<void> {

            scope::func_scope const& new_func;
            ast::node::function_definition const& func_def;

            func_definer(scope::func_scope const& f, ast::node::function_definition const& d)
                : new_func(f), func_def(d)
            {}

            result_type operator()(scope::global_scope const& s) const
            {
                auto const new_func_var = symbol::make<symbol::var_symbol>(func_def, func_def->name, true /*immutable*/);
                new_func_var->type = new_func->type;
                new_func_var->is_global = true;
                s->define_function(new_func);
                s->force_define_constant(new_func_var);
            }

            result_type operator()(scope::local_scope const& s) const
            {
                s->define_unnamed_func(new_func);
            }

            result_type operator()(scope::class_scope const& s) const
            {
                // TODO:
                // Add a instance variable of the member function

                // Note:
                // All member functions are defined in global scope to resolve them by overload resolution.
                auto const enclosing_scope
                    = apply_lambda(
                            [](auto const& ws) -> scope::any_scope { return ws.lock(); },
                            s->enclosing_scope
                        );
                boost::apply_visitor(*this, enclosing_scope);
            }

            [[noreturn]]
            result_type operator()(scope::func_scope const&) const
            {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }

        } definer{new_func, func_def};

        boost::apply_visitor(definer, current_scope);

        introduce_scope_and_walk(std::move(new_func), w);

        // Note:
        // Replace argument type of main function (#31)
        if (new_func->is_main_func() && new_func->params.size() > 0) {
            auto const& p = new_func->params[0];
            // Note:
            // Strict check for 'main' function will be done in semantics::detail::symbol_analyzer.
            if (p->type.is_template()) {
                p->type
                    = type::make<type::array_type>(
                            type::get_builtin_type("string", type::no_opt)
                        );
                func_def->params[0]->type = p->type;
            }

            if (!p->immutable) {
                semantic_error(func_def, "  Argument of main function '" + p->name + "' must be immutable");
                return;
            }
        }
    }

    void visit_class_var_decl(ast::node::variable_decl const& decl, scope::class_scope const& scope)
    {
        if (decl->is_instance_var()) {
            semantic_error(decl, "  '@' is not needed to declare instance variable here");
            return;
        }

        auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name, !decl->is_var);
        decl->symbol = new_var;

        // Set type if the type of variable is specified
        if (decl->maybe_type) {
            new_var->type
                = type::from_ast(*decl->maybe_type, current_scope);
        } else {
            new_var->type
                = type::make<type::template_type>(decl);
        }

        if (!scope->define_variable(new_var)) {
            failed++;
        }

        new_var->is_public = decl->is_public();
    }

    void visit_instance_var_init_decl(ast::node::variable_decl const& decl)
    {
        auto const f = with_current_scope(
                    [](auto const& s){ return s->get_enclosing_func(); }
                );

        if (!f || !(*f)->is_ctor()) {
            semantic_error(
                    decl,
                    "  Instance variable '" + decl->name + "' can be initialized only in constructor"
                );
            return;
        }

        auto const& ctor = *f;
        assert(ctor->is_member_func);
        decl->self_symbol = ctor->params[0];
    }

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& w)
    {
        if (auto const maybe_class = get_as<scope::class_scope>(current_scope)) {
            visit_class_var_decl(decl, *maybe_class);
        } else if (decl->is_instance_var()) {
            visit_instance_var_init_decl(decl);
        }

        w();
    }

    void check_init_instance_param(
            ast::node::parameter const& param,
            scope::func_scope const& member_func_scope,
            symbol::var_symbol const& param_sym)
    {
        if (!member_func_scope->is_ctor()) {
            semantic_error(param, "  Instance variable initializer '" + param->name + "' is only permitted in constructor's parameter.");
            return;
        }

        auto const maybe_weak_scope = get_as<scope::weak_class_scope>(member_func_scope->enclosing_scope);
        if (!maybe_weak_scope) {
            semantic_error(param, "  Instance variable initializer '" + param->name + "' is not permitted outside class definition.");
            return;
        }

        auto const& scope = maybe_weak_scope->lock();

        auto const instance_var = scope->resolve_instance_var(param->name.substr(1)/*Omit '@'*/);
        if (!instance_var) {
            semantic_error(param, boost::format("  Instance variable '%1%' in parameter doesn't exist in class '%2%'.") % param->name % scope->name);
            return;
        }

        if (param_sym->type && !(*instance_var)->type.is_template()) {
            if (param_sym->type != (*instance_var)->type) {
                semantic_error(param,
                        boost::format(
                            "  Type of instance variable '%1%' in parameter doesn't match.\n"
                            "  Note: The parameter type is '%2%' but the instance variable's type is actually '%3%'."
                        ) % param->name % param_sym->type.to_string() % (*instance_var)->type.to_string()
                    );
            }
        } else {
            param_sym->type = (*instance_var)->type;
            param->type = param_sym->type;
        }
    }

    auto get_param_sym(ast::node::parameter const& param)
    {
        // Note:
        // When the param's name is "_", it means unused.
        // Unique number (the address of 'param') is used instead of "_" as its name.
        // This is because "_" variable should be ignored by symbol resolution and
        // duplication check; it means that duplication of "_" must be permitted.
        // Defining the symbol is not skipped because of overload resolution. Parameters
        // must have its symbol and type for overloading the function.
        auto const new_param_sym =
            symbol::make<symbol::var_symbol>(
                param,
                param->name == "_" ? std::to_string(reinterpret_cast<size_t>(param.get())) : param->name,
                !param->is_var
            );
        param->param_symbol = new_param_sym;

        if (param->param_type) {
            // XXX:
            // type_calculator requires class information which should be analyzed forward.
            param->type = type::from_ast(*param->param_type, current_scope);
            if (!param->type) {
                semantic_error(
                        param,
                        boost::format("  Invalid type '%1%' for parameter '%2%'")
                            % apply_lambda([](auto const& t){
                                    return t->to_string();
                            }, *param->param_type)
                            % param->name
                    );
            }
            new_param_sym->type = param->type;
        }

        return new_param_sym;
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& w)
    {
        if (auto const maybe_func = get_as<scope::func_scope>(current_scope)) {
            auto const& func = *maybe_func;
            auto const new_param_sym = get_param_sym(param);

            if (param->is_instance_var_init()) {
                check_init_instance_param(param, func, new_param_sym);
            } else if (!param->param_type) {
                param->type = type::make<type::template_type>(param);
                new_param_sym->type = param->type;
            }

            if (!func->define_param(new_param_sym)) {
                failed++;
                return;
            }

        } else if (auto maybe_local = get_as<scope::local_scope>(current_scope)) {
            if (param->is_instance_var_init()) {
                semantic_error(param, "  Instance variable initializer '" + param->name + "' is not permitted here.");
            }

            // Note:
            // Enter here when the param is a variable to iterate in 'for' statement

            // XXX:
            // Do nothing
            // Symbol is defined in analyzer::visit(ast::node::for_stmt) for 'for' statement.
            // This is because it requires a range of for to get a type of variable to iterate.

        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        w();
    }

    template<class Walker>
    void visit(ast::node::for_stmt const& for_, Walker const& w)
    {
        w();

        assert(!for_->body_stmts->scope.expired());
        auto const child_scope = for_->body_stmts->scope.lock();

        for (auto const& i : for_->iter_vars) {
            assert(i->param_symbol.expired());
            auto const sym = get_param_sym(i);
            if (!child_scope->define_variable(sym)) {
                failed++;
                return;
            }
        }
    }

    template<class Walker>
    void visit(ast::node::lambda_expr const& lambda, Walker const&)
    {
        lambda->def->name = get_lambda_name(lambda);
        ast::walk_topdown(lambda->def, *this);
    }

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const& w)
    {
        if ((ret->line == 0u) && (ret->col == 0u)) {
            assert(ret->ret_exprs.size() > 0u);
            apply_lambda([&ret](auto const& child){ ret->set_source_location(*child); }, ret->ret_exprs[0]);
        }
        w();
    }

    // TODO: class scopes and member function scopes
    template<class Walker>
    void visit(ast::node::class_definition const& class_def, Walker const& w)
    {
        if (boost::algorithm::starts_with(class_def->name, "__builtin_")) {
            semantic_error(
                    class_def,
                    "  Only built-in classes' names are permitted to prefix '__builtin_'"
                );
            return;
        }

        auto new_class = scope::make<scope::class_scope>(class_def, current_scope, class_def->name);
        class_def->scope = new_class;
        new_class->type = type::make<type::class_type>(new_class);

        auto const maybe_global_scope = get_as<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto const& global = *maybe_global_scope;

        global->define_class(new_class);

        introduce_scope_and_walk(new_class, w);

        failed += check_functions_duplication(
                    new_class->member_func_scopes,
                    "class scope '" + class_def->name + "'"
                );

        auto const new_class_var = symbol::make<symbol::var_symbol>(class_def, class_def->name, true /*immutable*/);
        new_class_var->type = new_class->type;
        new_class_var->is_global = true;

        // Note:
        // Do not check the duplication of the variable because it is
        // checked by class duplication check.
        global->force_define_constant(new_class_var);
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& w)
    {
        // Note:
        // Replace '@foo()' with 'self.foo()'
        // This is necessary because replacing '@foo' with 'self.foo' makes
        // (self.foo)() from @foo().  So, replacing '@foo()' with 'self.foo()'
        // should be done before the replacement of '@foo'.
        if (auto const v = get_as<ast::node::var_ref>(invocation->child)) {
            auto const& var = *v;
            if (var->is_instance_var()) {
                var->name = var->name.substr(1u); // omit '@'
                invocation->args.insert(std::begin(invocation->args), generate_self_ref(*invocation));
            }
        }

        if (invocation->is_begin_end || invocation->is_let) {
            auto const lambda = *get_as<ast::node::lambda_expr>(invocation->child);
            lambda->set_source_location(*invocation);
            lambda->def->set_source_location(*invocation);
        }

        w();
    }

    template<class Walker>
    void visit(ast::node::any_expr &expr, Walker const& w)
    {
        if (auto const v = get_as<ast::node::var_ref>(expr)) {
            auto const& var = *v;
            if (var->is_instance_var()) {
                expr = generate_self_member_access(var);
            }
        }

        w();
    }

    template<class T, class Walker>
    void visit(T const&, Walker const& walker)
    {
        // Simply visit children recursively
        walker();
    }

};

} // namespace detail

template<class Node, class Scope>
std::size_t dispatch_forward_analyzer(Node &node, Scope const& scope_root)
{
    // Generate scope tree
    detail::forward_symbol_analyzer forward_resolver{scope_root};
    ast::walk_topdown(node, forward_resolver);

    return forward_resolver.failed;
}

// TODO:
// Consider class scope.  Now global scope is only considered.
template<class Node, class Scope>
Scope analyze_ast_node_forward(Node &node, Scope const& scope_root)
{
    {
        std::size_t const failed = dispatch_forward_analyzer(node, scope_root);
        if (failed > 0) {
            throw dachs::semantic_check_error{failed, "forward symbol resolution"};
        }
    }

    return scope_root;
}

} // namespace semantics
} // namespace dachs


#endif    // DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
