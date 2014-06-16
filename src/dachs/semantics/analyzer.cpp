#include <vector>
#include <string>
#include <cassert>
#include <iterator>
#include <unordered_set>
#include <tuple>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/range/algorithm/transform.hpp>

#include "dachs/exception.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/ast/ast_copier.hpp"
#include "dachs/semantics/analyzer_common.hpp"
#include "dachs/semantics/analyzer.hpp"
#include "dachs/semantics/forward_analyzer_impl.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::has;
using helper::variant::apply_lambda;
using helper::any_of;

struct return_types_gatherer {
    std::vector<type::type> result_types;
    std::vector<ast::node::return_stmt> failed_return_stmts;

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const&)
    {
        if (!ret->ret_type) {
            failed_return_stmts.push_back(ret);
        } else {
            result_types.push_back(ret->ret_type);
        }
    }

    template<class T, class W>
    void visit(T const&, W const& w)
    {
        w();
    }
};

struct weak_ptr_locker : public boost::static_visitor<scope::any_scope> {
    template<class WeakScope>
    scope::any_scope operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock();
    }
};

struct recursive_function_return_type_resolver {
    std::vector<type::type> result;

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const&)
    {
        if (ret->ret_type) {
            result.push_back(ret->ret_type);
        }
    }

    template<class Node, class Walker>
    void visit(Node const&, Walker const& recursive_walker)
    {
        recursive_walker();
    }
};

// Walk to resolve symbol references
class symbol_analyzer {

    scope::any_scope current_scope;
    scope::global_scope const global;

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

    // TODO:
    // Share this function in func_scope and member_func_scope
    template<class FuncDefNode, class EnclosingScope>
    std::pair<FuncDefNode, scope::func_scope>
    instantiate_function_from_template(
            FuncDefNode const& func_template_def,
            std::vector<type::type> const& arg_types,
            EnclosingScope const& enclosing_scope
        ) noexcept
    {
        assert(already_visited_functions.find(func_template_def) != std::end(already_visited_functions));

        auto instantiated_func_def = ast::copy_ast(func_template_def);

        // Note: No need to check functions duplication
        // Note: Type of parameters are analyzed here
        failed += dispatch_forward_analyzer(instantiated_func_def, enclosing_scope);
        assert(!instantiated_func_def->scope.expired());
        auto instantiated_func_scope = instantiated_func_def->scope.lock();

        // Replace types of params with instantiated types
        {
            assert(instantiated_func_def->params.size() == arg_types.size());
            auto def_param_itr = std::begin(instantiated_func_def->params);
            auto const def_param_end = std::end(instantiated_func_def->params);
            auto arg_itr = arg_types.cbegin();
            auto scope_param_itr = std::begin(instantiated_func_scope->params);
            for(; def_param_itr != def_param_end; ++def_param_itr, ++arg_itr, ++scope_param_itr) {
                if ((*def_param_itr)->type.is_template()) {
                    (*def_param_itr)->type = *arg_itr;
                    (*scope_param_itr)->type = *arg_itr;
                } else if ((*def_param_itr)->type != *arg_itr) {
                    // Note:
                    // Never reaches here because type mismatch is already detected in overload resolution
                    DACHS_RAISE_INTERNAL_COMPILATION_ERROR
                }
            }
        }

        // Last, symnol analyzer visits
        {
            auto saved_current_scope = current_scope;
            current_scope = enclosing_scope;
            ast::walk_topdown(instantiated_func_def, *this);
            already_visited_functions.insert(instantiated_func_def);
            current_scope = std::move(saved_current_scope);
        }

        assert(!instantiated_func_def->is_template());

        // Add instantiated function to function template node in AST
        func_template_def->instantiated.push_back(instantiated_func_def);

        return std::make_pair(instantiated_func_def, instantiated_func_scope);
    }

    void visit_func_parameter(ast::node::parameter const& param, scope::func_scope const& scope)
    {
        // Type is already set in forward_symbol_analyzer
        assert(param->type);

        auto new_param = symbol::make<symbol::var_symbol>(param, param->name, !param->is_var);
        new_param->type = param->type;
        param->param_symbol = new_param;

        if (!scope->define_param(new_param)) {
            failed++;
            return;
        }
    }

public:

    size_t failed = 0u;
    std::unordered_set<ast::node::function_definition> already_visited_functions;

    template<class Scope>
    explicit symbol_analyzer(Scope const& root, scope::global_scope const& global) noexcept
        : current_scope{root}, global{global}
    {}

    template<class Scope>
    explicit symbol_analyzer(Scope const& root, scope::global_scope const& global, decltype(already_visited_functions) const& fs) noexcept
        : current_scope{root}, global{global}, already_visited_functions(fs)
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
        if (already_visited_functions.find(func) != std::end(already_visited_functions)) {
            if (func->ret_type) {
                return;
            }
            recursive_function_return_type_resolver resolver;
            auto func_ = func; // To remove const
            ast::make_walker(resolver).walk(func_);
            if (resolver.result.empty()) {
                semantic_error(func, boost::format("Can't deduce return type of function '%1%' from return statement") % func->name);
            } else if (!boost::algorithm::all_of(resolver.result, [&](auto const& t){ return resolver.result[0] == t; })) {
                std::string note = "";
                for (auto const& t : resolver.result) {
                    note += '\'' + t.to_string() + "' ";
                }
                semantic_error(func, boost::format("Conflict among some return types in function '%1%'\nNote: Candidates are: " + note) % func->name);
            } else {
                func->ret_type = resolver.result[0];
            }
            return;
        }
        already_visited_functions.insert(func);

        if (func->is_template()) {
            // XXX:
            // Visit only parameters if function template for overload resolution.
            // This is because type checking and symbol analysis are needed in
            // not function templates but instantiated functions.
            return;
        }

        assert(!func->scope.expired());
        with_new_scope(func->scope.lock(), recursive_walker);

        // Deduce return type

        // TODO:
        // When return the function itself recursively, type deduction fails.
        // If return type is not valid, it means
        //   1. error occurs
        //   2. recursive call in return statement
        // If 2. , compiler should ignore the invalid type and deduce type from other return statements

        return_types_gatherer gatherer;
        auto func_ = func; // to remove const
        ast::make_walker(gatherer).walk(func_);

        if (!gatherer.failed_return_stmts.empty()) {
            semantic_error(func, boost::format("Can't deduce return type of function '%1%' from return statement\nNote: return statement is here: line%2%, col%3%") % func->name % gatherer.failed_return_stmts[0]->line % gatherer.failed_return_stmts[0]->col);
            return;
        }

        type::any_type const unit_type = type::make<type::tuple_type>();
        if (!gatherer.result_types.empty()) {
            if (func->kind == ast::symbol::func_kind::proc) {
                if (gatherer.result_types.size() != 1 || gatherer.result_types[0] != unit_type) {
                    semantic_error(func, boost::format("proc '%1%' can't return any value") % func->name);
                    return;
                }
            }
            // TODO:
            // Consider return statement without any value
            if (boost::algorithm::all_of(gatherer.result_types, [&](auto const& t){ return gatherer.result_types[0] == t; })) {
                auto const& deduced_type = gatherer.result_types[0];
                if (func->ret_type && *func->ret_type != deduced_type) {
                    semantic_error(func, boost::format("Return type of function '%1%' mismatch\nNote: Specified type is '%2%'\nNote: Deduced type is '%3%'") % func->name % func->ret_type->to_string() % deduced_type.to_string());
                } else {
                    func->ret_type = deduced_type;
                }
            } else {
                semantic_error(func, boost::format("Mismatch among the result types of return statements in function '%1%'") % func->name);
            }
        } else {
            if (func->ret_type && *func->ret_type != unit_type) {
                semantic_error(func, boost::format("Return type of function '%1%' mismatch\nNote: Specified type is '%2%'\nNote: Deduced type is '%3%'") % func->name % func->ret_type->to_string() % unit_type.to_string());
            } else {
                func->ret_type = unit_type;
            }
        }
    }
    // }}}

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& recursive_walker)
    {
        auto const visit_decl =
            [this, &decl](auto &scope)
            {
                auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name, !decl->is_var);
                decl->symbol = new_var;

                // Set type if the type of variable is specified
                if (decl->maybe_type) {
                    new_var->type
                        = boost::apply_visitor(
                            type_calculator_from_type_nodes{current_scope},
                            *(decl->maybe_type)
                        );
                }

                if (!scope->define_variable(new_var)) {
                    failed++;
                }
            };

        // Note:
        // I can't use apply_lambda() because some scopes don't have define_variable() member
        if (auto maybe_global_scope = get_as<scope::global_scope>(current_scope)) {
            visit_decl(*maybe_global_scope);
        } else if (auto maybe_local = get_as<scope::local_scope>(current_scope)) {
            visit_decl(*maybe_local);
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
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

        primary_lit->type = type::get_builtin_type(boost::apply_visitor(selector, primary_lit->value), type::no_opt);
    }

    template<class Walker>
    void visit(ast::node::symbol_literal const& sym_lit, Walker const& /*unused because it doesn't have child*/)
    {
        sym_lit->type = type::get_builtin_type("symbol", type::no_opt);
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
            auto arg0_type = type_of(arr_lit->element_exprs[0]);
            if (!boost::algorithm::all_of(
                    arr_lit->element_exprs,
                    [&](auto const& e){ return arg0_type == type_of(e); })
                ) {
                semantic_error(arr_lit, boost::format("All types of array elements must be '%1%'") % arg0_type.to_string());
                return;
            }
            arr_lit->type = type::make<type::array_type>(arg0_type);
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
            auto key_type_elem0 = type_of(dict_lit->value[0].first);
            auto value_type_elem0 = type_of(dict_lit->value[0].second);
            if (!boost::algorithm::all_of(
                    dict_lit->value,
                    [&](auto const& v)
                    {
                        // TODO:
                        // More comrehensive error message
                        return type_of(v.first) == key_type_elem0
                            && type_of(v.second) == value_type_elem0;
                    })
                ) {
                semantic_error(dict_lit,
                               boost::format("Type of keys or values mismatches\nNote: Key type is '%1%' and value type is '%2%'")
                               % key_type_elem0.to_string()
                               % value_type_elem0.to_string());
                return;
            }
            dict_lit->type = type::make<type::dict_type>(key_type_elem0, value_type_elem0);
        }
    }

    template<class Walker>
    void visit(ast::node::index_access const& access, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const child_type = type_of(access->child);
        auto const index_type = type_of(access->index_expr);

        if (!child_type || !index_type) {
            return;
        }

        // TODO
        // Below implementation is temporary.
        // Resolve operator [] function and get the return type of it.

        if (auto const maybe_array_type = type::get<type::array_type>(child_type)) {
            auto const& array_type = *maybe_array_type;
            if (array_type->element_type != type::get_builtin_type("int", type::no_opt)
                && array_type->element_type != type::get_builtin_type("uint", type::no_opt)) {
                semantic_error(access, boost::format("Index of array must be int or uint but actually '%1%'") % array_type->element_type.to_string());
                return;
            }
            access->type = array_type->element_type;
        } else if (auto const maybe_tuple_type = type::get<type::tuple_type>(child_type)) {
            auto const& tuple_type = *maybe_tuple_type;

            auto const maybe_primary_literal = get_as<ast::node::primary_literal>(access->index_expr);
            if (!maybe_primary_literal ||
                    (!has<int>((*maybe_primary_literal)->value)
                     && !has<uint>((*maybe_primary_literal)->value))
               ) {
                semantic_error(access, "Index of tuple must be int or uint literal");
                return;
            }

            auto const& literal = (*maybe_primary_literal)->value;
            auto const out_of_bounds = [this, &access](auto const i){ semantic_error(access, boost::format("Index access is out of bounds\nNote: Index is %1%") % i); };
            if (auto const maybe_int_lit = get_as<int>(literal)) {
                auto const idx = *maybe_int_lit;
                if (idx < 0 || static_cast<unsigned int>(idx) >= tuple_type->element_types.size()) {
                    out_of_bounds(*maybe_int_lit);
                    return;
                }
                access->type = tuple_type->element_types[idx];
            } else if (auto const maybe_uint_lit = get_as<unsigned int>(literal)) {
                if (*maybe_uint_lit >= tuple_type->element_types.size()) {
                    out_of_bounds(*maybe_uint_lit);
                    return;
                }
                access->type = tuple_type->element_types[*maybe_uint_lit];
            } else {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }
        } else if (auto const maybe_dict_type = type::get<type::dict_type>(child_type)) {
            auto const& dict_type = *maybe_dict_type;
            if (index_type != dict_type->key_type) {
                semantic_error(
                    access,
                    boost::format("Index type of dictionary mismatches\nNote: Expected '%1%' but actually '%2%'")
                    % dict_type->key_type.to_string()
                    % index_type.to_string()
                );
                return;
            }
            access->type = dict_type->value_type;
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }
    }

    template<class Walker>
    void visit(ast::node::binary_expr const& bin_expr, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const lhs_type = type_of(bin_expr->lhs);
        auto const rhs_type = type_of(bin_expr->rhs);

        if (!lhs_type || !rhs_type) {
            return;
        }

        // TODO:
        // Resolve oeprator function overload and get the result type.
        // Below implementation is temporary.

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

        // TODO:
        // Find operator function and get the result type of it
        if (any_of({"==", "!=", ">", "<", ">=", "<="}, bin_expr->op)) {
            bin_expr->type = type::get_builtin_type("bool", type::no_opt);
        } else if (bin_expr->op == "&&" || bin_expr->op == "||") {
            if (lhs_type != type::get_builtin_type("bool", type::no_opt)) {
                semantic_error(bin_expr, boost::format("Opeartor '%1%' only takes bool type operand\nNote: Operand type is '%2%'") % bin_expr->op % lhs_type.to_string());
            }
            bin_expr->type = type::get_builtin_type("bool", type::no_opt);
        } else if (bin_expr->op == ".." || bin_expr->op == "...") {
            // TODO: Range type
            throw not_implemented_error{bin_expr, __FILE__, __func__, __LINE__, "builtin range type"};
        } else {
            bin_expr->type = lhs_type;
        }
    }

    template<class Walker>
    void visit(ast::node::unary_expr const& unary, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const operand_type = type_of(unary->expr);

        if (!operand_type) {
            return;
        }

        if (unary->op == "!") {
            if (operand_type != type::get_builtin_type("bool", type::no_opt)) {
                semantic_error(unary, boost::format("Opeartor '%1%' only takes bool type operand\nNote: Operand type is '%2%'") % unary->op % operand_type.to_string());
            }
            unary->type = type::get_builtin_type("bool", type::no_opt);
        } else {
            unary->type = operand_type;
        }
    }

    template<class Walker>
    void visit(ast::node::if_expr const& if_, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const condition_type = type_of(if_->condition_expr);
        auto const then_type = type_of(if_->then_expr);
        auto const else_type = type_of(if_->else_expr);

        if (!condition_type || !then_type || !else_type) {
            return;
        }

        if (condition_type != type::get_builtin_type("bool", type::no_opt)) {
            semantic_error(if_, boost::format("Type of condition in if expression must be bool\nNote: Type of condition is '%1%'") % condition_type.to_string());
            return;
        }

        if (then_type != else_type) {
            semantic_error(if_, boost::format("Type mismatch between type of then clause and else clause\nNote: Type of then clause is '%1%'\nNote: Type of else clause is '%2%'") % then_type.to_string() % else_type.to_string());
            return;
        }

        if_->type = then_type;
    }

    // TODO:
    // If return type of function is not determined, interrupt current parsing and parse the
    // function at first.  Add the function list which are already visited and check it at
    // function_definition node
    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& recursive_walker)
    {
        recursive_walker();

        auto maybe_var_ref = get_as<ast::node::var_ref>(invocation->child);
        if (!maybe_var_ref) {
            throw not_implemented_error{invocation, __FILE__, __func__, __LINE__, "function variable invocation"};
        }

        auto const& var_ref = *maybe_var_ref;
        if (!var_ref->type) {
            return;
        }

        std::string const& name = var_ref->name;

        if (!type::has<type::func_ref_type>(var_ref->type)) {
            semantic_error(invocation
                         , boost::format("'%1%' is not a function or function reference\nNote: Type of %1% is %2%")
                            % name
                            % var_ref->type.to_string()
                        );
            return;
        }

        std::vector<type::type> arg_types;
        arg_types.reserve(invocation->args.size());
        // Get type list of arguments
        boost::transform(invocation->args, std::back_inserter(arg_types), [](auto const& e){ return type_of(e);});

        for (auto const& arg_type : arg_types) {
            if (!arg_type) {
                return;
            }
        }

        auto maybe_func =
            apply_lambda(
                    [&](auto const& s)
                    {
                        return s->resolve_func(name, arg_types);
                    }, current_scope
                );

        if (!maybe_func) {
            semantic_error(invocation, boost::format("Function '%1%' is not found") % name);
            return;
        }

        auto func = *maybe_func;
        auto func_def = func->get_ast_node();

        if (func->is_template()) {
            // Note:
            // No need to use apply_lambda for func->enclosing_scope
            // because enclosing_scope of func_scope is always global_scope.

            std::tie(func_def, func) = instantiate_function_from_template(func_def, arg_types, global);

            assert(!global->ast_root.expired());
        }

        if (!func_def->ret_type) {
            auto saved_current_scope = current_scope;
            current_scope = global; // enclosing scope of function scope is always global scope
            ast::walk_topdown(func_def, *this);
            current_scope = std::move(saved_current_scope);
        }

        if (auto maybe_ret_type = func_def->ret_type) {
            invocation->type = *maybe_ret_type;
        } else {
            semantic_error(invocation, boost::format("Cannot deduce the return type of function '%1%'") % func->to_string());
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

        auto const actual_type = type_of(typed->child_expr);
        if (actual_type != typed->type) {
            semantic_error(typed, boost::format("Type mismatch.  Specified '%1%' but actually typed to '%2%'") % typed->type % actual_type);
            return;
        }

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
    void visit(ast::node::member_access const& member, Walker const& /*unused*/)
    {
        throw not_implemented_error{member, __FILE__, __func__, __LINE__, "member access"};
    }

    template<class Walker>
    void visit(ast::node::object_construct const& obj, Walker const& /*unused*/)
    {
        obj->type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, obj->obj_type);
        throw not_implemented_error{obj, __FILE__, __func__, __LINE__, "object construction"};
    }

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const& recursive_walker)
    {
        recursive_walker();

        if (ret->ret_exprs.size() == 1) {
            // When return statement has one expression, its return type is the same as it
            auto t = type_of(ret->ret_exprs[0]);
            if (!t) {
                return;
            }
            ret->ret_type = t;
        } else {
            // Otherwise its return type is tuple
            auto tuple_type = type::make<type::tuple_type>();
            for (auto const& e : ret->ret_exprs) {
                auto t = type_of(e);
                if (!t) {
                    return;
                }
                tuple_type->element_types.push_back(t);
            }
            ret->ret_type = tuple_type;
        }
    }

    template<class Walker>
    void visit(ast::node::for_stmt const& for_, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const range_t = type_of(for_->range_expr);
        if (!range_t) {
            return;
        }

        auto const substitute_param_type =
            [this](auto const& param, auto const& t)
            {
                if (param->type) {
                    if (param->type != t) {
                        semantic_error(
                                param,
                                boost::format("Type of '%1%' mismatch\nNote: Type of '%1%' is '%2%' but the range requires '%3%'")
                                    % param->name
                                    % param->type.to_string()
                                    % type::to_string(t)
                            );
                    }
                } else {
                    param->type = t;
                    param->param_symbol.lock()->type = t;
                }
            };

        auto const check_element =
            [this, &for_, &substitute_param_type](auto const& elem_type)
            {
                if (auto const maybe_elem_tuple_type = type::get<type::tuple_type>(elem_type)) {
                    auto const& elem_tuple_type = *maybe_elem_tuple_type;
                    if (elem_tuple_type->element_types.size() != for_->iter_vars.size()) {
                        semantic_error(for_, boost::format("Number of variables and elements of range in 'for' statement mismatches\nNote: %1% variables but %2% elements") % for_->iter_vars.size() % elem_tuple_type->element_types.size());
                        return;
                    }

                    auto param_itr = std::begin(for_->iter_vars);
                    auto const param_end = std::end(for_->iter_vars);
                    auto elem_type_itr = elem_tuple_type->element_types.cbegin();
                    for (;param_itr != param_end; ++param_itr, ++elem_type_itr) {
                        substitute_param_type(*param_itr, *elem_type_itr);
                    }
                } else {
                    if (for_->iter_vars.size() != 1) {
                        semantic_error(for_, "Number of a variable here in 'for' statement must be 1");
                        return;
                    }
                    substitute_param_type(for_->iter_vars[0], elem_type);
                }
            };

        if (auto const maybe_array_range_type = type::get<type::array_type>(range_t)) {
            check_element((*maybe_array_range_type)->element_type);
        } else if (auto const maybe_range_type = type::get<type::range_type>(range_t)) {
            check_element((*maybe_range_type)->element_type);
        } else if (auto const maybe_dict_range_type = type::get<type::dict_type>(range_t)) {
            auto const& dict_range_type = *maybe_dict_range_type;
            if (for_->iter_vars.size() != 2) {
                semantic_error(for_, boost::format("2 iteration variable is needed to iterate dictionary '%1%'") % dict_range_type->to_string());
                return;
            }
            substitute_param_type(for_->iter_vars[0], dict_range_type->key_type);
            substitute_param_type(for_->iter_vars[1], dict_range_type->value_type);
        } else {
            semantic_error(for_, boost::format("Range to iterate in for statement must be range, array or dictionary but actually '%1%'") % range_t.to_string());
        }
    }

    void check_condition_expr(ast::node::any_expr const& expr)
    {
        apply_lambda(
            [this](auto const& e)
            {
                if (e->type != type::get_builtin_type("bool", type::no_opt)) {
                    semantic_error(e, boost::format("Type of condition must be bool but actually '%1%'") % e->type.to_string());
                }
            }
            , expr
        );
    }

    template<class Walker>
    void visit(ast::node::while_stmt const& while_, Walker const& recursive_walker)
    {
        recursive_walker();
        check_condition_expr(while_->condition);
    }

    template<class Walker>
    void visit(ast::node::postfix_if_stmt const& postfix_if, Walker const& recursive_walker)
    {
        recursive_walker();
        check_condition_expr(postfix_if->condition);
    }

    template<class Walker>
    void visit(ast::node::case_stmt const& case_, Walker const& recursive_walker)
    {
        recursive_walker();
        for (auto const& when : case_->when_stmts_list) {
            check_condition_expr(when.first);
        }
    }

    template<class Walker>
    void visit(ast::node::switch_stmt const& switch_, Walker const& recursive_walker)
    {
        recursive_walker();
        auto const switcher_type = type_of(switch_->target_expr);
        for (auto const& when : switch_->when_stmts_list) {
            for (auto const& cond : when.first) {
                auto const t = type_of(cond);
                if (t != switcher_type) {
                    apply_lambda(
                            [this, &t, &switcher_type](auto const& node)
                            {
                                semantic_error(
                                    node,
                                    boost::format("Type of 'when' condition must be '%1%' but actually '%2%'")
                                        % switcher_type.to_string()
                                        % t.to_string()
                                    );
                            }, cond
                        );
                }
            }
        }
    }

    template<class Walker>
    void visit(ast::node::initialize_stmt const& init, Walker const& recursive_walker)
    {
        assert(init->var_decls.size() > 0);

        auto const substitute_type =
            [this, &init](auto const& symbol, auto const& t)
            {
                if (symbol->type) {
                    if (symbol->type != t) {
                        semantic_error(
                                init,
                                boost::format("Types mismatch on substituting '%1%'\nNote: Type of '%1%' is '%2%' but rhs type is '%3%'")
                                    % symbol->name
                                    % symbol->type.to_string()
                                    % type::to_string(t)
                            );
                    }
                } else {
                    symbol->type = t;
                }
            };

        recursive_walker();

        if (!init->maybe_rhs_exprs) {
            for (auto const& v : init->var_decls) {
                assert(!v->symbol.expired());
                if (!v->symbol.lock()->type) {
                    semantic_error(init, boost::format("Type of '%1%' can't be deduced") % v->name);
                }
            }
            return;
        }

        auto &rhs_exprs = *init->maybe_rhs_exprs;
        assert(rhs_exprs.size() > 0);

        for (auto const& e : rhs_exprs) {
            if (!type_of(e)) {
                return;
            }
        }

        if (init->var_decls.size() == 1) {
            auto const& var_sym = init->var_decls[0]->symbol.lock();
            if (rhs_exprs.size() == 1) {
                substitute_type(var_sym, type_of(rhs_exprs[0]));
            } else {
                auto const rhs_type = type::make<type::tuple_type>();
                rhs_type->element_types.reserve(rhs_exprs.size());
                for (auto const& e : rhs_exprs) {
                    rhs_type->element_types.push_back(type_of(e));
                }
                substitute_type(var_sym, rhs_type);
            }
        } else {
            if (rhs_exprs.size() == 1) {
                auto maybe_rhs_type = type::get<type::tuple_type>(type_of(rhs_exprs[0]));
                if (!maybe_rhs_type) {
                    semantic_error(init, boost::format("Rhs type of initialization here must be tuple\nNote: Rhs type is %1%") % type_of(rhs_exprs[0]).to_string());
                    return;
                }

                auto const rhs_type = *maybe_rhs_type;
                if (rhs_type->element_types.size() != init->var_decls.size()) {
                    semantic_error(init, boost::format("Size of elements in lhs and rhs mismatch\nNote: Size of lhs is %1%, size of rhs is %2%") % init->var_decls.size() % rhs_type->element_types.size());
                    return;
                }

                auto tuple_itr = rhs_type->element_types.cbegin();
                auto const tuple_end = rhs_type->element_types.cend();
                auto decl_itr = std::begin(init->var_decls);
                for (; tuple_itr != tuple_end; ++tuple_itr, ++decl_itr) {
                    substitute_type((*decl_itr)->symbol.lock(), *tuple_itr);
                }
            } else {
                if (init->var_decls.size() != rhs_exprs.size()) {
                    semantic_error(init, boost::format("Size of elements in lhs and rhs mismatch\nNote: Size of lhs is %1%, size of rhs is %2%") % init->var_decls.size() % rhs_exprs.size());
                    return;
                }

                auto lhs_itr = std::begin(init->var_decls);
                auto const lhs_end = std::end(init->var_decls);
                auto rhs_itr = rhs_exprs.cbegin();
                for (; lhs_itr != lhs_end; ++lhs_itr, ++rhs_itr) {
                    substitute_type((*lhs_itr)->symbol.lock(), type_of(*rhs_itr));
                }
            }
        }
    }

    template<class Walker>
    void visit(ast::node::assignment_stmt const& assign, Walker const& recursive_walker)
    {
        recursive_walker();

        for (auto const& es : {assign->assignees, assign->rhs_exprs}) {
            for (auto const& e : es) {
                if (!type_of(e)) {
                    return;
                }
            }
        }

        // TODO:
        // Check assignability of lhs expressions.
        // If attempting to assign to immutable variable, raise an error.

        auto const check_types =
            [this, &assign](auto const& t1, auto const& t2)
            {
                if (t1 != t2) {
                    semantic_error(
                            assign,
                            boost::format("Types mismatch on assignment\nNote: Type of lhs is '%1%, type of rhs is '%2%'")
                                % type::to_string(t1)
                                % type::to_string(t2)
                        );
                }
            };

        if (assign->assignees.size() == 1) {
            auto const assignee_type = type_of(assign->assignees[0]);
            if (assign->rhs_exprs.size() == 1) {
                auto const rhs_type = type_of(assign->rhs_exprs[0]);
                check_types(assignee_type, rhs_type);
            } else {
                auto const maybe_tuple_type = type::get<type::tuple_type>(assignee_type);
                if (!maybe_tuple_type) {
                    semantic_error(assign, boost::format("Assignee's type here must be tuple but actually '%1%'") % assignee_type.to_string());
                    return;
                }

                auto lhs_type_itr = (*maybe_tuple_type)->element_types.cbegin();
                auto rhs_expr_itr = assign->rhs_exprs.cbegin();
                auto const rhs_expr_end = assign->rhs_exprs.cend();
                for (; rhs_expr_itr != rhs_expr_end; ++lhs_type_itr, ++rhs_expr_itr) {
                    check_types(*lhs_type_itr, type_of(*rhs_expr_itr));
                }
            }
        } else {
            if (assign->rhs_exprs.size() == 1) {
                auto const assigner_type = type_of(assign->rhs_exprs[0]);
                auto const maybe_tuple_type = type::get<type::tuple_type>(assigner_type);
                if (!maybe_tuple_type) {
                    semantic_error(assign, boost::format("Assigner's type here must be tuple but actually '%1%'") % assigner_type.to_string());
                    return;
                }

                auto rhs_type_itr = (*maybe_tuple_type)->element_types.cbegin();
                auto lhs_expr_itr = assign->assignees.cbegin();
                auto const lhs_expr_end = assign->assignees.cend();
                for (; lhs_expr_itr != lhs_expr_end; ++rhs_type_itr, ++lhs_expr_itr) {
                    check_types(type_of(*lhs_expr_itr), *rhs_type_itr);
                }
            } else {
                if (assign->assignees.size() != assign->rhs_exprs.size()) {
                    semantic_error(assign, boost::format("Size of assignees and assigners mismatches\nNote: Size of assignee is %1%, size of assigner is %2%") % assign->assignees.size() % assign->rhs_exprs.size());
                    return;
                }

                auto lhs_itr = assign->assignees.cbegin();
                auto const lhs_end = assign->assignees.cend();
                auto rhs_itr = assign->rhs_exprs.cbegin();
                for (; lhs_itr != lhs_end; ++lhs_itr, ++rhs_itr) {
                    check_types(type_of(*lhs_itr), type_of(*rhs_itr));
                }
            }
        }
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

void check_semantics(ast::ast &a, scope::scope_tree &t)
{
    detail::symbol_analyzer resolver{t.root, t.root};
    ast::walk_topdown(a.root, resolver);

    if (resolver.failed > 0) {
        throw dachs::semantic_check_error{resolver.failed, "symbol resolution"};
    }
}

} // namespace semantics
} // namespace dachs
