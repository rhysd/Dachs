#if !defined DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
#define      DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED

#include <iterator>
#include <algorithm>
#include <unordered_set>
#include <cstddef>
#include <cassert>

#include <boost/format.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/parser/importer.hpp"
#include "dachs/semantics/forward_analyzer.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/type_from_ast.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/ast/ast_copier.hpp"
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
    syntax::importer &importer;

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

    std::string get_lambda_name(ast::node::lambda_expr const& lambda) const noexcept
    {
        auto const& l = lambda->location;
        return "lambda."
            + std::to_string(l.line)
            + '.' + std::to_string(l.col)
            + '.' + std::to_string(l.length)
            + '.' + helper::hex_string_of_ptr(lambda->def.get());
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
                                "  Note: Previous definition is at %3%"
                            ) % situation % (*right)->to_string() % lhs_def->location
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
                                "  Note: Previous definition is at %2%"
                            ) % (*right)->name % (*left)->location
                        );
                    failed++;
                }
            }
        }

        return failed;
    }

    template<class Funcs>
    std::size_t check_operator_function_args(Funcs const& functions) const
    {
        std::unordered_set<std::string> const
            unary_only = {"~", "!"},
            binary_only = {
                ">>" , "<<" , "<="
              , ">=" , "==" , "!="
              , "&&" , "||" , "*"
              , "/"  , "%"  , "<"
              , ">"  , "&"  , "^"
              , "|"  , "[]"
            },
            ternary_only = {
                "[]="
            },
            unary_or_binary{"+", "-"}
        ;

        std::size_t failed = 0u;

        auto const in =
            [](auto const& set, auto const& op)
            {
                return set.find(op) != std::end(set);
            };

        auto const operator_arg_error =
            [&failed, this](auto const& f, auto const& msg)
            {
                output_semantic_error(
                        f->get_ast_node(),
                        "  Operator '" + f->name + "' must have just " + msg
                    );
                ++failed;
            };

        for (auto const& f : functions) {
            auto const s = f->params.size();

            if (in(unary_only, f->name) && s != 1u) {
                operator_arg_error(f, "1 parameter");
            } else if (in(binary_only, f->name) && s != 2u) {
                operator_arg_error(f, "2 parameters");
            } else if (in(ternary_only, f->name) && s != 3u) {
                operator_arg_error(f, "3 parameters");
            } else if (in(unary_or_binary, f->name) && s != 1u && s != 2u) {
                operator_arg_error(f, "1 or 2 parameter(s)");
            }
        }

        return failed;
    }

    template<class Funcs>
    std::size_t check_cast_funcs_duplication(Funcs const& cast_funcs) const
    {
        std::size_t failed = 0u;

        auto const error
            = [&failed, this](auto const& f, auto && msg)
            {
                output_semantic_error(
                        f->get_ast_node(),
                        std::forward<decltype(msg)>(msg)
                    );
                ++failed;
            };

        for (auto const& f : cast_funcs) {
            assert(f->ret_type);

            if (f->params.size() != 1u) {
                error(
                    f,
                    "  Wrong number of parameters ("
                        + std::to_string(f->params.size())
                        + " for 1).  Cast function must have only one parameter."
                );
                continue;
            }

            if (type::is_a<type::template_type>(f->params[0]->type)) {
                error(f, "  Cast function must know its type of parameter.  Specify the type of parameter explicitly.");
            }

            if (!f->params[0]->type.is_aggregate() && !f->ret_type->is_aggregate()) {
                error(f, "  Cast from built-in type to built-in type can't be defined.");
            }
        }

        if (failed != 0u) {
            return failed;
        }

        // Note:
        // Check the duplication of cast functions.
        // Note:
        // Can't use check_functions_duplication() because it overloads by its return type.
        auto const end = cast_funcs.cend();
        for (auto left = cast_funcs.cbegin(); left != end; ++left) {
            for (auto right = std::next(left); right != end; ++right) {
                auto const& r = *right;
                auto const& l = *left;
                if (r->params[0]->type == l->params[0]->type && r->ret_type == l->ret_type) {
                    auto const ldef = l->get_ast_node();
                    output_semantic_error(
                        r->get_ast_node(),
                        boost::format(
                            "  Cast function is redefined.\n"
                            "  Note: Cast from '%1%' to '%2%'.\n"
                            "  Note: Previous definition is at %3%"
                        ) % r->params[0]->type.to_string()
                          % r->ret_type->to_string()
                          % ldef->location
                    );
                    ++failed;
                }
            }
        }

        return failed;
    }

    template<class Location>
    ast::node::var_ref generate_self_ref(Location const& location) const
    {
        auto const ref = ast::make<ast::node::var_ref>("self");
        ref->set_source_location(location);
        return ref;
    }

    ast::node::ufcs_invocation generate_self_member_access(ast::node::var_ref const& v) const
    {
        auto const self = generate_self_ref(*v);
        auto const access = ast::make<ast::node::ufcs_invocation>(self, v->name.substr(1u));
        access->set_source_location(*v);
        return access;
    }

    template<class String, class Location>
    ast::node::parameter generate_receiver_node(String const& class_name, Location const& location) const
    {
        auto const receiver_type_node = ast::make<ast::node::primary_type>(class_name);
        receiver_type_node->set_source_location(location);

        auto const receiver_node
            = ast::make<ast::node::parameter>(
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
            = ast::make<ast::node::function_definition>(
                ast::node_type::function_definition::ctor_tag{},
                params,
                ast::make<ast::node::statement_block>()
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
                    ast::make<ast::node::parameter>(
                            true,
                            '@' + d->name,
                            boost::none
                        )
                );
            params.back()->set_source_location(*def);
        }

        auto ctor_def
            = ast::make<ast::node::function_definition>(
                ast::node_type::function_definition::ctor_tag{},
                params,
                ast::make<ast::node::statement_block>()
            );
        ctor_def->set_source_location(*def);

        def->member_funcs.emplace_back(std::move(ctor_def));
    }

    void grow_main_arg_type_ast(ast::node::parameter const& p)
    {
        if (!p->param_type) {
            auto argv = ast::make<ast::node::primary_type>("argv");
            argv->set_source_location(*p);
            p->param_type = std::move(argv);
        }
    }

    template<class TypeNode>
    auto from_ast(TypeNode const& node)
    {
        return type::from_ast<decltype(*this)>(node, current_scope);
    }

public:

    size_t failed;

    template<class Scope>
    forward_symbol_analyzer(Scope const& s, syntax::importer &i) noexcept
        : current_scope(s), importer(i), failed(0)
    {}

    scope::class_scope define_new_class(ast::node::class_definition const& c, scope::global_scope const& global)
    {
        auto const new_class = scope::make<scope::class_scope>(c, current_scope, c->name);
        c->scope = new_class;
        new_class->type = type::make<type::class_type>(new_class);
        global->define_class(new_class);

        auto const new_class_var = symbol::make<symbol::var_symbol>(c, c->name, true /*immutable*/);
        new_class_var->type = new_class->type;
        new_class_var->is_global = true;

        // Note:
        // Do not check the duplication of the variable because it is
        // checked by class duplication check.
        global->force_define_constant(new_class_var);

        return new_class;
    }

    template<class Walker>
    void visit(ast::node::inu const& inu, Walker const& w)
    {
        importer.import(inu);

        auto const maybe_global_scope = get_as<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto const& global = *maybe_global_scope;

        // Note:
        // Add receiver parameter to member functions' parameters here because this operation makes side effect to AST
        // and it causes a problem when re-visiting class_definition to instantiate class template
        // if this operation is done at visiting class_definition.
        for (auto const& c : inu->classes) {

            // Note: Define all classes before visiting all functions (including member functions)
            define_new_class(c, global);

            // Note: Other preprocesses
            {
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

        failed += check_functions_duplication(global->functions, "global scope");
        failed += check_classes_duplication(inu->classes);
        failed += check_operator_function_args(global->functions);
        failed += check_cast_funcs_duplication(global->cast_funcs);
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
    void visit(ast::node::block_expr const& block, Walker const& w)
    {
        auto new_local_scope = scope::make<scope::local_scope>(current_scope);
        block->scope = new_local_scope;
        if (auto const parent = get_as<scope::local_scope>(current_scope)) {
            (*parent)->define_child(new_local_scope);
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

        if (func_def->is_main_func() && func_def->params.size() > 0) {
            // Note:
            // Strict check for 'main' function will be done in semantics::detail::symbol_analyzer.
            grow_main_arg_type_ast(func_def->params[0]);
        }

        // Define scope
        auto new_func = scope::make<scope::func_scope>(func_def, current_scope, func_def->name);
        new_func->type = type::make<type::generic_func_type>(scope::weak_func_scope{new_func});
        func_def->scope = new_func;

        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            semantic_error(func_def, "  Procedure '" + func_def->name + "' can't have return type");
            return;
        }

        // Note:
        // Get return type for checking duplication of overloaded function
        if (func_def->return_type) {
            auto const result = from_ast(*func_def->return_type);

            if (auto const error = result.get_error()) {
                semantic_error(
                        func_def,
                        boost::format("  Invalid type '%1%' is specified in return type of function '%2%'")
                            % *error % func_def->name
                    );
                return;
            }

            func_def->ret_type = result.get_unsafe();
            new_func->ret_type = result.get_unsafe();
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
            auto const result = from_ast(*decl->maybe_type);

            if (auto const error = result.get_error()) {
                semantic_error(
                        decl,
                        boost::format("  Invalid type '%1%' is specified in declaration of variable '%2%'")
                            % *error % decl->name
                    );
                return;
            }

            new_var->type = result.get_unsafe();
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

        auto const& instance_var_type = (*instance_var)->type;

        if (!param_sym->type) {
            param_sym->type = instance_var_type;
            param->type = param_sym->type;
            return;
        }

        if (type::is_a<type::template_type>(instance_var_type)) {
            return;
        }

        if (!type::fuzzy_match(param_sym->type, instance_var_type)) {
            semantic_error(param,
                    boost::format(
                        "  Type of instance variable '%1%' in parameter doesn't match.\n"
                        "  Note: The parameter type is '%2%' but the instance variable's type is actually '%3%'."
                    ) % param->name % param_sym->type.to_string() % instance_var_type.to_string()
                );
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
            from_ast(*param->param_type).apply(

                    [&](auto const& success)
                    {
                        param->type = success;
                        new_param_sym->type = success;
                    },

                    [&, this](auto const& failure)
                    {
                        semantic_error(
                                param,
                                boost::format("  Invalid type '%1%' is specified in parameter '%2%'")
                                    % failure % param->name
                            );
                    }

                );
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
        if (ret->location.empty()) {
            assert(ret->ret_exprs.size() > 0u);
            apply_lambda([&ret](auto const& child){ ret->set_source_location(*child); }, ret->ret_exprs[0]);
        }
        w();
    }

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

        auto const scope = [&class_def, this]
        {
            // Note:
            // At the first time of forward analysis, all class scope are generated at
            // visiting ast::node::inu because ast::from_ast() requires a class definition
            // to generate type::class_type.
            // However, when typeof({expr}) is introduced, it is hard to use ast::from_ast() in
            // forward analysis because typeof({expr}) needs to evaluate an expression.
            if (class_def->scope.expired()) {
                auto const global = get_as<scope::global_scope>(current_scope);
                assert(global);
                return define_new_class(class_def, *global);
            } else {
                return class_def->scope.lock();
            }
        }();

        introduce_scope_and_walk(scope, w);

        failed += check_functions_duplication(
                    scope->member_func_scopes,
                    "class scope '" + class_def->name + "'"
                );
    }

    // Note:
    // Replace '@foo()' with 'self.foo()'
    // This is necessary because replacing '@foo' with 'self.foo' makes
    // (self.foo)() from @foo().  So, replacing '@foo()' with 'self.foo()'
    // should be done before the replacement of '@foo'.
    void modify_member_func_invocation_ast(ast::node::func_invocation const& invocation, ast::node::var_ref const& child_var)
    {
        if (!child_var->is_instance_var()) {
            return;
        }

        auto const member_name = child_var->name.substr(1u); // omit '@'

        auto const self_var
                = with_current_scope(
                    [](auto const& s){ return s->resolve_var("self"); }
                );

        if (!self_var) {
            return;
        }

        // Note:
        // If the instance var access is really variable access, it should not be modified.
        //
        // class X
        //   f : func() : ()
        //
        //   func foo
        //     @f()
        //   end
        // end
        //
        // In above, @f should be member access and should not be modified to self.f().
        // So, at first check the identifier refers function or variable, then modify it
        // from @foo() to self.foo() only if it refers function.
        if (auto const clazz = type::get<type::class_type>((*self_var)->type)) {
            if ((*clazz)->ref.lock()->resolve_instance_var(member_name)) {
                return;
            }
        }

        child_var->name = member_name;
        invocation->args.insert(std::begin(invocation->args), generate_self_ref(*invocation));
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& w)
    {
        // Note:
        // Replace '@foo()' with 'self.foo()'
        // This is necessary because replacing '@foo' with 'self.foo' makes
        // (self.foo)() from @foo().  So, replacing '@foo()' with 'self.foo()'
        // should be done before the replacement of '@foo'.
        if (auto const var = get_as<ast::node::var_ref>(invocation->child)) {
            modify_member_func_invocation_ast(invocation, *var);
        }

        w();
    }

    // Note:
    // t: (int, char, string) -> t[0]: int, t[1]: char, t[2]: string
    template<class Expr>
    std::vector<ast::node::any_expr> break_up_tuple_access(Expr const& tuple_expr, std::size_t const num_elems)
    {
        auto const location = ast::node::location_of(tuple_expr);
        std::vector<ast::node::any_expr> expanded;
        expanded.reserve(num_elems);

        for (unsigned int const i : helper::indices(num_elems)) {
            auto index_constant = ast::make<ast::node::primary_literal>(i);
            index_constant->location = location;

            auto access = ast::make<ast::node::index_access>(tuple_expr, std::move(index_constant));
            access->location = location;
            expanded.emplace_back(std::move(access));
        }

        return expanded;
    }

    // Note:
    // foo += bar  -> foo = foo + bar
    // This function returns rhs binary expression
    ast::node::binary_expr solve_compound_assign(
            ast::node::any_expr const& lhs,
            ast::node::any_expr && rhs,
            std::string const& op)
    {
        auto location = ast::node::location_of(rhs);
        auto copied_lhs = ast::copy_ast(lhs);
        auto const bin = ast::make<ast::node::binary_expr>(std::move(copied_lhs), op, std::move(rhs));
        bin->location = std::move(location);
        return bin;
    }

    template<class Walker>
    void visit(ast::node::assignment_stmt const& assign, Walker const& w)
    {
        {
            auto const lhs_size = assign->assignees.size();
            auto const rhs_size = assign->rhs_exprs.size();

            if (lhs_size == 1) {
                if (rhs_size != 1) {
                    semantic_error(
                            assign,
                            "  Assigning multiple values to a tuple is not permitted.  Use tuple literal for rhs of assignment"
                        );
                    return;
                }
            } else if (rhs_size == 1) {
                assign->rhs_tuple_expansion = true;
                assign->rhs_exprs = break_up_tuple_access(assign->rhs_exprs[0], lhs_size);

                assert(assign->assignees.size() == assign->rhs_exprs.size());
            } else if (lhs_size != rhs_size) {
                semantic_error(
                        assign,
                        boost::format(
                            "  The number of lhs and rhs in assignment mismatches\n"
                            "  Note: The number of lhs is '%1%' and the one of rhs is '%2%'"
                        ) % lhs_size % rhs_size
                    );
                return;
            }
        }

        assert(assign->assignees.size() == assign->rhs_exprs.size());

        if (assign->op != "=") {
            // Note: At compound assignment
            auto const binary_op = assign->op.substr(0, assign->op.size()-1);

            for (auto const i : helper::indices(assign->rhs_exprs)) {
                assign->rhs_exprs[i] = solve_compound_assign(assign->assignees[i], std::move(assign->rhs_exprs[i]), binary_op);
            }

            assign->op = "=";
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
std::size_t dispatch_forward_analyzer(Node &node, Scope const& scope_root, syntax::importer &i)
{
    // Generate scope tree
    detail::forward_symbol_analyzer forward_resolver{scope_root, i};
    ast::walk_topdown(node, forward_resolver);

    return forward_resolver.failed;
}

// TODO:
// Consider class scope.  Now global scope is only considered.
template<class Node, class Scope>
Scope analyze_ast_node_forward(Node &node, Scope const& scope_root, syntax::importer &i)
{
    {
        std::size_t const failed = dispatch_forward_analyzer(node, scope_root, i);
        if (failed > 0) {
            throw dachs::semantic_check_error{failed, "forward symbol resolution"};
        }
    }

    return scope_root;
}

} // namespace semantics
} // namespace dachs


#endif    // DACHS_SEMANTICS_FOWARD_ANALYZER_IMPL_HPP_INCLUDED
