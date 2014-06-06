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
using helper::variant::apply_lambda;
using helper::any_of;

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

struct weak_ptr_locker : public boost::static_visitor<scope::any_scope> {
    template<class WeakScope>
    scope::any_scope operator()(WeakScope const& w) const
    {
        assert(!w.expired());
        return w.lock();
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

        // Replace types of params with instantiated types
        {
            assert(instantiated_func_def->params.size() == arg_types.size());
            auto inst_itr = std::begin(instantiated_func_def->params);
            auto arg_itr = arg_types.cbegin();
            auto const inst_end = std::end(instantiated_func_def->params);
            for(; inst_itr != inst_end; ++inst_itr, ++arg_itr) {
                if ((*inst_itr)->type.is_template()) {
                    (*inst_itr)->type = *arg_itr;
                }
            }
        }
        auto instantiated_func_scope = instantiated_func_def->scope.lock();

        // Last, symnol analyzer visits
        {
            auto saved_current_scope = current_scope;
            current_scope = enclosing_scope;
            ast::walk_topdown(instantiated_func_def, *this);
            already_visited_functions.insert(instantiated_func_def);
            current_scope = saved_current_scope;
        }

        assert(!instantiated_func_def->is_template());

        // Add instantiated function to function template node in AST
        func_template_def->instantiated.push_back(instantiated_func_def);

        return std::make_pair(instantiated_func_def, instantiated_func_scope);
    }

    void visit_func_parameter(ast::node::parameter const& param, scope::func_scope const& scope)
    {
        auto new_param = symbol::make<symbol::var_symbol>(param, param->name);
        new_param->type = param->type;
        param->param_symbol = new_param;

        if (!scope->define_param(new_param)) {
            failed++;
            return;
        }

        // Type is already set in forward_symbol_analyzer
        assert(param->type);
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
            return;
        }
        already_visited_functions.insert(func);

        if (func->is_template()) {
            // XXX:
            // Visit only parameters if function template for overload resolution.
            // This is because type checking and symbol analysis are needed in
            // not function templates but instantiated functions.
            for (auto const& p : func->params) {
                visit_func_parameter(p, func->scope.lock());
            }
            return;
        }

        assert(!func->scope.expired());
        with_new_scope(func->scope.lock(), recursive_walker);

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
            // TODO:
            // If there is no return statement, the result type should be ()
            func->ret_type = unit_type;
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
        if (auto maybe_func = get_as<scope::func_scope>(current_scope)) {
            visit_func_parameter(param, *maybe_func);
        } else if (auto maybe_local = get_as<scope::local_scope>(current_scope)) {
            // When for statement
            auto new_param = symbol::make<symbol::var_symbol>(param, param->name);
            new_param->type = param->type;
            param->param_symbol = new_param;
            auto& local = *maybe_local;

            if (!local->define_local_var(new_param)) {
                failed++;
                return;
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        recursive_walker();
    }

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& recursive_walker)
    {
        auto maybe_local = get_as<scope::local_scope>(current_scope);
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
            new_var->type = decl->type;
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
