#include <cassert>
#include <cstddef>
#include <iterator>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/range/algorithm/find_if.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/algorithm/cxx11/all_of.hpp> // For boost::all_of
#include <boost/range/numeric.hpp>

#include "dachs/scope.hpp"
#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"
#include "dachs/ast_walker.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {

namespace scope_node {

namespace detail {

template<class FuncScope, class Args, class RetType>
inline
std::size_t get_overloaded_function_score(FuncScope const& func, Args const& args, RetType const& ret)
{
    if (args.size() != func->params.size()) {
        return 0u;
    }

    std::size_t score = 1u;
    auto const maybe_func_def = ast::node::get_shared_as<ast::node::function_definition>(func->ast_node);
    assert(maybe_func_def);
    auto const& func_def = *maybe_func_def;

    // Score of return type. (Room to consider remains)
    if (ret && func_def->ret_type) {
        if (*ret == *(func_def->ret_type)) {
            score *= 2u;
        } else {
            score *= 0u;
        }
    }

    // Return type doesn't match.  No need to calculate the score of arguments' coincidence.
    if (score == 0u) {
        return 0u;
    }

    if (args.size() == 0) {
        return score;
    }

    // Calculate the score of arguments' coincidence
    std::vector<std::size_t> v;
    v.reserve(args.size());

    boost::transform(args, func_def->params, std::back_inserter(v),
            [](auto const& arg, auto const& param)
            {
                assert(arg);
                if (param->template_type_ref) {
                    // Function parameter is template.  It matches any types.
                    return 1u;
                }

                assert(param->type);

                if (*param->type == arg) {
                    return 2u;
                } else {
                    return 0u;
                }
            });

    // Note:
    // If the function have no argument and return type is not specified, score is 1.
    return boost::accumulate(v, score, [](auto l, auto r){ return l*r; });
}

} // namespace detail

boost::optional<scope::func_scope>
global_scope::resolve_func( std::string const& name
            , std::vector<type::type> const& args
            , boost::optional<type::type> const& ret_type) const
{
    boost::optional<scope::func_scope> result = boost::none;

    std::size_t score = 0u;
    for (auto const& f : functions) {
        if (f->name != name) {
            continue;
        }

        auto const score_tmp = detail::get_overloaded_function_score(f, args, ret_type);
        if (score_tmp > score) {
            score = score_tmp;
            result = f;
        }
    }

    return result;
}

ast::node::function_definition func_scope::get_ast_node() const noexcept
{
    auto maybe_func_def = ast::node::get_shared_as<ast::node::function_definition>(ast_node);
    assert(maybe_func_def);
    return *maybe_func_def;
}

} // namespace scope_node

namespace scope {

namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::apply_lambda;

template<class Variant>
inline type::type type_of(Variant const& v) noexcept
{
    return apply_lambda([](auto const& n){ return n->type; }, v);
}

class type_calculator_from_type_nodes
    : public boost::static_visitor<type::type> {

    any_scope const& current_scope;

    template<class T>
    type::type apply_recursively(T const& t) const noexcept
    {
        return boost::apply_visitor(*this, t);
    }

public:

    explicit type_calculator_from_type_nodes(any_scope const& c)
        : current_scope(c)
    {}

    type::type operator()(ast::node::primary_type const& t) const noexcept
    {
        auto const builtin = type::get_builtin_type(t->template_name.c_str());
        if (builtin) {
            return *builtin;
        } else {
            auto const c = boost::apply_visitor(class_resolver{t->template_name}, current_scope);
            // TODO: deal with exception
            assert(c && "This assertion is temporary");
            auto const ret = type::make<type::class_type>(t->template_name, *c);
            for (auto const& instantiated : t->instantiated_templates) {
                ret->holder_types.push_back(apply_recursively(instantiated));
            }
            return ret;
        }
    }

    type::type operator()(ast::node::array_type const& t) const noexcept
    {
        return type::make<type::array_type>(
                    apply_recursively(t->elem_type)
                );
    }

    type::type operator()(ast::node::tuple_type const& t) const noexcept
    {
        auto const ret = type::make<type::tuple_type>();
        ret->element_types.reserve(t->arg_types.size());
        for (auto const& arg : t->arg_types) {
            ret->element_types.push_back(apply_recursively(arg));
        }
        return ret;
    }

    type::type operator()(ast::node::dict_type const& t) const noexcept
    {
        return type::make<type::dict_type>(
                    apply_recursively(t->key_type),
                    apply_recursively(t->value_type)
                );
    }

    type::type operator()(ast::node::qualified_type const& t) const noexcept
    {
        type::qualifier new_qualifier;
        switch (t->qualifier) {
        case ast::symbol::qualifier::maybe:
            new_qualifier = type::qualifier::maybe;
            break;
        default:
            assert(false);
            break;
        }

        return type::make<type::qualified_type>(
                    new_qualifier, apply_recursively(t->type)
               );
    }

    type::type operator()(ast::node::func_type const& t) const noexcept
    {
        std::vector<type::any_type> param_types;
        param_types.reserve(t->arg_types.size());
        for (auto const& a : t->arg_types) {
            param_types.push_back(apply_recursively(a));
        }

        if (t->ret_type) {
            return {type::make<type::func_type>(
                    std::move(param_types),
                    apply_recursively(*(t->ret_type))
                )};
        } else {
            return {type::make<type::proc_type>(std::move(param_types))};
        }
    }
};

struct return_types_gatherer {
    std::vector<type::type> result_types;
    std::vector<ast::node::return_stmt> failed_return_stmts;

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const&)
    {
        if (ret->ret_exprs.size() == 1) {
            // When return statement has one expression, its return type is the same as it
            auto t = type_of(ret->ret_exprs[0]);
            if (!t) {
                failed_return_stmts.push_back(ret);
                return;
            }
            result_types.push_back(t);
        } else {
            // Otherwise its return type is tuple
            auto ret_type = type::make<type::tuple_type>();
            for (auto const& e : ret->ret_exprs) {
                auto t = type_of(e);
                if (!t) {
                    failed_return_stmts.push_back(ret);
                    return;
                }
                ret_type->element_types.push_back(t);
            }
            result_types.push_back(ret_type);
        }
    }

    template<class T, class W>
    void visit(T const&, W const& w)
    {
        w();
    }
};

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

    template<class Node, class Message>
    void semantic_error(Node const& n, Message const& msg) noexcept
    {
        output_semantic_error(n, msg);
        failed++;
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
        if (auto maybe_local_scope = get_as<local_scope>(current_scope)) {
            auto &enclosing_scope = *maybe_local_scope;
            enclosing_scope->define_child(new_local_scope);
        } else if (auto maybe_func_scope = get_as<func_scope>(current_scope)) {
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
        // Define scope
        auto maybe_global_scope = get_as<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_func = make<func_scope>(func_def, global_scope, func_def->name);
        new_func->type = type::make<type::func_ref_type>(scope::weak_func_scope{new_func});
        func_def->scope = new_func;

        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            semantic_error(func_def, "TODO");
            return;
        }

        if (func_def->return_type) {
            func_def->ret_type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, *(func_def->return_type));
        }


        auto new_func_var = symbol::make<symbol::var_symbol>(func_def, func_def->name);
        new_func_var->type = new_func->type;
        if (global_scope->define_function(new_func)) {
            // If the symbol passes duplication check, it is also defined as variable
            global_scope->define_global_constant(new_func_var);
            with_new_scope(std::move(new_func), recursive_walker);
        } else {
            failed++;
        }
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

    template<class Node, class Message>
    void semantic_error(Node const& n, Message const& msg) noexcept
    {
        output_semantic_error(n, msg);
        failed++;
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

        return_types_gatherer gatherer;
        auto func_ = func; // to remove const
        ast::make_walker(gatherer).walk(func_);

        if (!gatherer.failed_return_stmts.empty()) {
            semantic_error(func, boost::format("Can't deduce return type of function '%1%'from return statement\nNote: return statement is here: line%2%, col%3%") % func->name % gatherer.failed_return_stmts[0]->line % gatherer.failed_return_stmts[0]->col);
            return;
        }

        if (!gatherer.result_types.empty()) {
            if (boost::algorithm::all_of(gatherer.result_types, [&](auto const& t){ return gatherer.result_types[0] == t; })) {
                func->ret_type = gatherer.result_types[0];
            } else {
                semantic_error(func, boost::format("Mismatch among the result types of return statements in function '%1%'") % func->name);
                return;
            }
        }
    }
    // }}}

    // Do not analyze in forward_symbol_analyzer because variables can't be referenced forward {{{
    template<class Walker>
    void visit(ast::node::constant_decl const& const_decl, Walker const& recursive_walker)
    {
        auto maybe_global_scope = get_as<scope::global_scope>(current_scope);
        assert(maybe_global_scope);
        auto& global_scope = *maybe_global_scope;
        auto new_var = symbol::make<symbol::var_symbol>(const_decl, const_decl->name);
        const_decl->symbol = new_var;
        if (!global_scope->define_global_constant(new_var)) {
            failed++;
        }
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& recursive_walker)
    {
        auto new_param = symbol::make<symbol::var_symbol>(param, param->name);
        param->param_symbol = new_param;
        if (auto maybe_func = get_as<func_scope>(current_scope)) {
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

        } else if (auto maybe_local = get_as<local_scope>(current_scope)) {
            // When for statement
            auto& local = *maybe_local;
            if (!local->define_local_var(new_param)) {
                failed++;
            }
        } else {
            assert(false);
        }

        // Add parameter type if specified
        if (param->param_type) {
            param->type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, *(param->param_type));
            new_param->type = *(param->type);
        }

        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& recursive_walker)
    {
        auto maybe_local = get_as<local_scope>(current_scope);
        assert(maybe_local);
        auto& local = *maybe_local;
        auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name);
        decl->symbol = new_var;
        if (!local->define_local_var(new_var)) {
            failed++;
        }

        // Set type if the type of variable is specified
        if (decl->maybe_type) {
            decl->type
                = boost::apply_visitor(
                    type_calculator_from_type_nodes{current_scope},
                    *(decl->maybe_type)
                );
            new_var->type = *(decl->type);
        }

        recursive_walker();
    }
    // }}}

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& recursive_walker)
    {
        auto maybe_var_symbol = boost::apply_visitor(scope::var_symbol_resolver{var->name}, current_scope);
        if (maybe_var_symbol) {
            var->symbol = *maybe_var_symbol;
            var->type = (*maybe_var_symbol)->type;
        } else {
            semantic_error(var, boost::format("Symbol '%1%' is not found") % var->name);
        }
        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::primary_literal const& primary_lit, Walker const& /*unused because it doesn't have child*/)
    {
        struct : public boost::static_visitor<char const* const> {

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

        auto const builtin = type::get_builtin_type(boost::apply_visitor(selector, primary_lit->value));
        assert(builtin);
        primary_lit->type = *builtin;
    }

    template<class Walker>
    void visit(ast::node::symbol_literal const& sym_lit, Walker const& /*unused because it doesn't have child*/)
    {
        auto const builtin = type::get_builtin_type("symbol");
        assert(builtin);
        sym_lit->type = *builtin;
    }

    template<class Walker>
    void visit(ast::node::array_literal const& arr_lit, Walker const& recursive_walker)
    {
        recursive_walker();
        // Note: Check only the head of element because Dachs doesn't allow implicit type conversion
        if (arr_lit->element_exprs.empty()) {
            if (!arr_lit->type) {
                semantic_error(arr_lit, "Empty array must be typed by ':'");
            }
        } else {
            arr_lit->type = type::make<type::array_type>(type_of(arr_lit->element_exprs[0]));
        }
    }

    template<class Walker>
    void visit(ast::node::tuple_literal const& tuple_lit, Walker const& recursive_walker)
    {
        if (tuple_lit->element_exprs.size() == 1) {
            semantic_error(tuple_lit, "Size of tuple should not be 1");
        }
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
        if (dict_lit->value.empty()) {
            if (!dict_lit->type) {
                semantic_error(dict_lit, "Empty dictionary must be typed by ':'");
            }
        } else {
            auto const& p = dict_lit->value[0];
            dict_lit->type = type::make<type::dict_type>(type_of(p.first), type_of(p.second));
        }
    }

    // TODO:
    // Calculate type from type nodes here because it requires forward analyzed information

    template<class Walker>
    void visit(ast::node::binary_expr const& bin_expr, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const lhs_type = type_of(bin_expr->lhs);
        auto const rhs_type = type_of(bin_expr->rhs);

        if (!lhs_type || !rhs_type) {
            return;
        }

        // TODO: Temporary
        // Now binary operator requires both side hands have the same type
        if (lhs_type != rhs_type) {
            semantic_error(
                    bin_expr,
                    boost::format("Type mismatch in binary operator '%1%'\nNote: Type of lhs is %2%\nNote: Type of rhs is %3%")
                        % bin_expr->op
                        % lhs_type.to_string()
                        % rhs_type.to_string()
                    );
            return;
        }

        if (bin_expr->op == ".." || bin_expr->op == "...") {
            bin_expr->type = type::make<type::range_type>(type_of(bin_expr->lhs), type_of(bin_expr->rhs));
        } else {
            // TODO:
            // Find operator function and get the result type of it
        }
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& recursive_walker)
    {
        recursive_walker();

        auto maybe_var_ref = get_as<ast::node::var_ref>(invocation->child);
        if (!maybe_var_ref) {
            throw not_implemented_error{__FILE__, __func__, __LINE__, "function variable invocation"};
        }

        if (!(*maybe_var_ref)->type) {
            return;
        }

        std::string const& name = (*maybe_var_ref)->name;

        if (!type::has<type::func_ref_type>((*maybe_var_ref)->type)) {
            semantic_error(invocation
                         , boost::format("'%1%' is not a function or function reference\nNote: Type of %1% is %2%")
                            % name
                            % (*maybe_var_ref)->type.to_string()
                        );
            return;
        }

        std::vector<type::type> arg_types;
        arg_types.reserve(invocation->args.size());
        // Get type list of arguments
        boost::transform(invocation->args, std::back_inserter(arg_types), [](auto const& e){ return detail::type_of(e);});

        for (auto const& arg_type : arg_types) {
            if (!arg_type) {
                return;
            }
        }

        if (auto maybe_func = apply_lambda([&](auto const& s){ return s->resolve_func(name, arg_types, boost::none/*TODO: Temporary*/); }, current_scope)) {
            auto &func = *maybe_func;
            if (auto maybe_ret_type = func->get_ast_node()->ret_type) {
                // TODO: Get return type from instantiated function
                invocation->type = *maybe_ret_type;
            } else {
                semantic_error(invocation, boost::format("cannot deduce the return type of function '%1%'") % func->name);
            }
        } else {
            semantic_error(invocation, boost::format("function '%1%' is not found") % name);
        }
    }

    template<class Walker>
    void visit(ast::node::typed_expr const& typed, Walker const& recursive_walker)
    {
        typed->type = boost::apply_visitor(
                            type_calculator_from_type_nodes{current_scope},
                            typed->specified_type
                        );

        recursive_walker();

        // TODO:
        // Implement a function to get type from type nodes in AST
        // TODO:
        // Check the type of child is the same as the one of lhs
        // TODO:
        // Use another visitor to set type and check types. Do not use recursive_walker().
    }

    template<class Walker>
    void visit(ast::node::cast_expr const& casted, Walker const& recursive_walker)
    {

        casted->type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, casted->casted_type);

        // TODO:
        // Find cast function

        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::member_access const& /*member_access*/, Walker const& /*unused*/)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "member access"};
    }

    template<class Walker>
    void visit(ast::node::object_construct const& obj, Walker const& /*unused*/)
    {
        obj->type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, obj->obj_type);
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
        print_func->define_param(symbol::make<symbol::var_symbol>(a.root, "value"));
        tree_root->define_function(print_func);
        tree_root->define_global_constant(symbol::make<symbol::var_symbol>(a.root, "print"));

        // Operators
        // cast functions
    }

    {
        // Builtin classes

        // range
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

    // TODO: Get type of global function variables' type on visit node::function_definition
    // Note:
    // Type of function can be set after parameters' types are set.
    // Should function type be set at forward analysis phase?
    // If so, type calculation pass should be separated from symbol analysis pass.

    return scope_tree{tree_root};
}

} // namespace scope
} // namespace dachs
