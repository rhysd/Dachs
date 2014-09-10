#include <vector>
#include <string>
#include <cassert>
#include <iterator>
#include <unordered_set>
#include <tuple>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/adaptor/transformed.hpp>

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
#include "dachs/semantics/tmp_member_checker.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/each.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::has;
using helper::variant::apply_lambda;
using boost::adaptors::transformed;

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

// TODO:
// If recursive call is used in 'if' expression, it fails to deduce
// func fib(n)
//     return (if n <= 1 then 1 else fib(n-1)+fib(n-2))
// end
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

struct var_ref_marker_for_lhs_of_assign {
    // Mark as lhs of assignment
    template<class W>
    void visit(ast::node::var_ref const& ref, W const&)
    {
        ref->is_lhs_of_assignment = true;
    }

    // Do not mark var ref in index access
    template<class W>
    void visit(ast::node::index_access const&, W const&)
    {}

    // Otherwise, simply visit the node
    template<class N, class W>
    void visit(N const&, W const& w)
    {
        w();
    }
};

struct var_ref_getter_for_lhs_of_assign {

    template<class... Args>
    ast::node::var_ref visit(boost::variant<Args...> const& v)
    {
        return apply_lambda([this](auto const& n){ return visit(n); }, v);
    }

    ast::node::var_ref visit(ast::node::var_ref const& ref)
    {
        return ref;
    }

    ast::node::var_ref visit(ast::node::index_access const& access)
    {
        return visit(access->child);
    }

    ast::node::var_ref visit(ast::node::typed_expr const& typed)
    {
        return visit(typed->child_expr);
    }

    template<class T>
    ast::node::var_ref visit(T const&)
    {
        return nullptr;
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
        assert(func_template_def->is_template());

        auto instantiated_func_def = ast::copy_ast(func_template_def);

        // Note: No need to check functions duplication
        // Note: Type of parameters are analyzed here
        failed += dispatch_forward_analyzer(instantiated_func_def, enclosing_scope);
        assert(!instantiated_func_def->scope.expired());
        auto instantiated_func_scope = instantiated_func_def->scope.lock();

        // Replace types of params with instantiated types
        {
            assert(instantiated_func_def->params.size() == arg_types.size());

            helper::each([](auto &def_param, auto const& arg, auto &scope_param)
                {
                    if (def_param->type.is_template()) {
                        def_param->type = arg;
                        scope_param->type = arg;
                    } else if (def_param->type != arg) {
                        // Note:
                        // Never reaches here because type mismatch is already detected in overload resolution
                        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
                    }
                }
                , instantiated_func_def->params
                , arg_types
                , instantiated_func_scope->params);
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
            if (func->ret_type || func->kind == ast::symbol::func_kind::proc) {
                return;
            }
            recursive_function_return_type_resolver resolver;
            auto func_ = func; // To remove const
            ast::make_walker(resolver).walk(func_);
            if (resolver.result.empty()) {
                semantic_error(func, boost::format("Can't deduce return type of function '%1%' from return statement") % func->name);
            } else if (boost::algorithm::any_of(resolver.result, [&](auto const& t){ return resolver.result[0] != t; })) {
                std::string note = "";
                for (auto const& t : resolver.result) {
                    note += '\'' + t.to_string() + "' ";
                }
                semantic_error(func, boost::format("Conflict among some return types in function '%1%'\nNote: Candidates are: " + note) % func->name);
            } else {
                func->ret_type = resolver.result[0];
                assert(!func->scope.expired());
                func->scope.lock()->ret_type = resolver.result[0];
            }
            return;
        }
        already_visited_functions.insert(func);

        bool const is_query_function = func->name.find('?') != std::string::npos;

        if (func->kind == ast::symbol::func_kind::proc && is_query_function) {
            semantic_error(func, boost::format("'?' can't be used in a name of procedure because procedure can't return value"));
            return;
        }

        if (func->is_template()) {
            // XXX:
            // Visit only parameters if function template for overload resolution.
            // This is because type checking and symbol analysis are needed in
            // not function templates but instantiated functions.
            return;
        }

        assert(!func->scope.expired());
        auto scope = func->scope.lock();
        with_new_scope(scope, recursive_walker);

        // Deduce return type

        return_types_gatherer gatherer;
        auto func_ = func; // to remove const
        ast::make_walker(gatherer).walk(func_);

        if (!gatherer.failed_return_stmts.empty()) {
            semantic_error(
                func,
                boost::format(
                    "Can't deduce return type of function '%1%' from return statement\n"
                    "Note: return statement is here: line%2%, col%3%")
                    % func->name
                    % gatherer.failed_return_stmts[0]->line
                    % gatherer.failed_return_stmts[0]->col
            );
            return;
        }

        if (!gatherer.result_types.empty()) {
            if (func->kind == ast::symbol::func_kind::proc) {
                if (gatherer.result_types.size() != 1 || gatherer.result_types[0] != type::get_unit_type()) {
                    semantic_error(func, boost::format("proc '%1%' can't return any value") % func->name);
                    return;
                }
            }

            if (boost::algorithm::any_of(
                        gatherer.result_types,
                        [&](auto const& t){ return gatherer.result_types[0] != t; })) {
                semantic_error(
                        func,
                        boost::format("Mismatch among the result types of return statements in function '%1%'")
                            % func->name
                    );
                return;
            }

            auto const& deduced_type = gatherer.result_types[0];
            if (func->ret_type && *func->ret_type != deduced_type) {
                semantic_error(
                        func,
                        boost::format(
                            "Return type of function '%1%' mismatch\n"
                            "Note: Specified type is '%2%'\n"
                            "Note: Deduced type is '%3%'")
                            % func->name
                            % func->ret_type->to_string()
                            % deduced_type.to_string()
                    );
                return;
            }

            func->ret_type = deduced_type;
            scope->ret_type = func->ret_type;
        } else {
            if (func->ret_type && *func->ret_type != type::get_unit_type()) {
                semantic_error(
                        func,
                        boost::format(
                            "Return type of function '%1%' mismatch\n"
                            "Note: Specified type is '%2%'\n"
                            "Note: Deduced type is '%3%'")
                            % func->name
                            % func->ret_type->to_string()
                            % type::get_unit_type()->to_string()
                    );
            }
            func->ret_type = type::get_unit_type();
            scope->ret_type = type::get_unit_type();
        }

        if (is_query_function && *func->ret_type != type::get_builtin_type("bool", type::no_opt)) {
            semantic_error(func, boost::format("function '%1%' must return bool because it includes '?' in its name") % func->name);
        }
    }
    // }}}

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& recursive_walker)
    {
        auto const visit_decl =
            [this, &decl](auto &scope)
            {
                if (decl->name == "_") {
                    return true;
                }

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
                    return false;
                }

                return true;
            };

        // Note:
        // I can't use apply_lambda() because some scopes don't have define_variable() member
        if (auto maybe_global_scope = get_as<scope::global_scope>(current_scope)) {
            if (!visit_decl(*maybe_global_scope)) {
                return;
            }
        } else if (auto maybe_local = get_as<scope::local_scope>(current_scope)) {
            if (!visit_decl(*maybe_local)) {
                return;
            }
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        recursive_walker();
    }
    // }}}

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& recursive_walker)
    {
        assert(var->name != "");

        if (var->is_lhs_of_assignment && var->name == "_") {
            assert(var->symbol.expired());
            return;
        }

        auto name = var->name;
        if (name.back() == '!') {
            name.pop_back();
        }

        auto maybe_var_symbol = boost::apply_visitor(scope::var_symbol_resolver{name}, current_scope);
        if (!maybe_var_symbol) {
            semantic_error(var, boost::format("Symbol '%1%' is not found") % name);
            return;
        }

        var->symbol = *maybe_var_symbol;
        var->type = (*maybe_var_symbol)->type;
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
        if (arr_lit->element_exprs.empty() && !arr_lit->type) {
            semantic_error(arr_lit, "Empty array must be typed by ':'");
            return;
        }

        auto arg0_type = type_of(arr_lit->element_exprs[0]);
        if (boost::algorithm::any_of(
                arr_lit->element_exprs,
                [&](auto const& e){ return arg0_type != type_of(e); })
            ) {
            semantic_error(arr_lit, boost::format("All types of array elements must be '%1%'") % arg0_type.to_string());
            return;
        }

        if (auto const& arg0_arr_type = type::get<type::array_type>(arg0_type)) {
            auto const knows_size =
                [](auto const& e) -> bool { return *(*type::get<type::array_type>(type_of(e)))->size; };

            if (boost::algorithm::all_of(
                    arr_lit->element_exprs,
                    knows_size
                )) {
                auto const arg0_size = *(*arg0_arr_type)->size;
                if (boost::algorithm::any_of(
                        arr_lit->element_exprs,
                        [arg0_size](auto const& e){ return *(*type::get<type::array_type>(type_of(e)))->size != arg0_size; }
                    )) {
                    semantic_error(arr_lit, "All array elements in an array must be the same length");
                    return;
                }

            } else if (boost::algorithm::any_of(
                    arr_lit->element_exprs,
                    knows_size
                )) {
                // Note:
                // Some elements' types don't have its length and at least one element's type has its length
                semantic_error(arr_lit, "Some array elements have fixed length and others don't have");
                return;
            }
        }

        arr_lit->type = type::make<type::array_type>(arg0_type, arr_lit->element_exprs.size());
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
        if (dict_lit->value.empty() && !dict_lit->type) {
            semantic_error(dict_lit, "Empty dictionary must be typed by ':'");
            return;
        }

        auto key_type_elem0 = type_of(dict_lit->value[0].first);
        auto value_type_elem0 = type_of(dict_lit->value[0].second);

        if (boost::algorithm::any_of(
                dict_lit->value,
                [&](auto const& v)
                {
                    return type_of(v.first) != key_type_elem0 || type_of(v.second) != value_type_elem0;
                })
            ) {
            semantic_error(
                    dict_lit,
                    boost::format("Type of keys or values mismatches\nNote: Key type is '%1%' and value type is '%2%'")
                    % key_type_elem0.to_string()
                    % value_type_elem0.to_string()
                );
            return;
        }

        dict_lit->type = type::make<type::dict_type>(key_type_elem0, value_type_elem0);
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
            if (index_type != type::get_builtin_type("int", type::no_opt)
                && index_type != type::get_builtin_type("uint", type::no_opt)) {
                semantic_error(
                        access,
                        boost::format("Index of array must be int or uint but actually '%1%'")
                            % index_type.to_string()
                    );
                return;
            }
            access->type = (*maybe_array_type)->element_type;
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
            auto const out_of_bounds
                = [this, &access](auto const i)
                {
                    semantic_error(access, boost::format("Index access is out of bounds\nNote: Index is %1%") % i);
                };
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
            semantic_error(
                access,
                boost::format("'%1%' type value is not accessible by index.") % child_type.to_string()
            );
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
        if (helper::any_of({"==", "!=", ">", "<", ">=", "<="}, bin_expr->op)) {
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

        // TODO:
        // Below is temporary implementation.
        // Resolve functions for unary operator and get the return type of it.

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
        auto maybe_var_ref = get_as<ast::node::var_ref>(invocation->child);
        if (!maybe_var_ref) {
            throw not_implemented_error{invocation, __FILE__, __func__, __LINE__, "function variable invocation"};
        }

        if ((*maybe_var_ref)->name.back() == '!') {
            invocation->is_monad_invocation = true;
            throw not_implemented_error{invocation, __FILE__, __func__, __LINE__, "function invocation with monad"};
        }

        recursive_walker();

        auto const var_sym = (*maybe_var_ref)->symbol.lock();
        if (!var_sym->type) {
            return;
        }

        if (!type::is_a<type::func_ref_type>(var_sym->type)) {
            semantic_error(invocation
                         , boost::format("'%1%' is not a function or function reference\nNote: Type of %1% is %2%")
                            % var_sym->name
                            % var_sym->type.to_string()
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
                        return s->resolve_func(var_sym->name, arg_types);
                    }, current_scope
                );

        if (!maybe_func) {
            semantic_error(invocation, boost::format("Function '%1%' is not found") % var_sym->name);
            return;
        }

        auto func = *maybe_func;

        if (func->is_builtin) {
            assert(func->ret_type);
            invocation->type = *func->ret_type;
            invocation->func_symbol = func;
            return;
        }

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

        invocation->func_symbol = func;
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
        recursive_walker();
        casted->type = boost::apply_visitor(type_calculator_from_type_nodes{current_scope}, casted->casted_type);

        // TODO:
        // Find cast function and get its result type
    }

    template<class Walker>
    void visit(ast::node::member_access const& member, Walker const& recursive_walker)
    {
        recursive_walker();

        auto const checked = check_member_var(member);
        if (auto const& error_msg = get_as<std::string>(checked)) {
            output_semantic_error(member, *error_msg);
            return;
        }

        auto const& t = get_as<type::type>(checked);
        assert(t);
        member->type = *t;
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
                                boost::format("Type of '%1%' mismatches\nNote: Type of '%1%' is '%2%' but the range requires '%3%'")
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

                    helper::each([&](auto &p, auto const& e){ substitute_param_type(p, e); }
                                , for_->iter_vars
                                , elem_tuple_type->element_types);
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
    void visit(ast::node::if_stmt const& if_, Walker const& recursive_walker)
    {
        recursive_walker();
        check_condition_expr(if_->condition);
        for (auto const& elseif : if_->elseif_stmts_list) {
            check_condition_expr(elseif.first);
        }
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

        recursive_walker();

        if (!init->maybe_rhs_exprs) {
            for (auto const& v : init->var_decls) {
                if (v->symbol.expired()) {
                    // When it fails to define the variable
                    return;
                }
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

        for (auto const& v : init->var_decls) {
            if (v->name != "_" && v->symbol.expired()) {
                return;
            }
        }

        auto const substitute_type =
            [this, &init](auto const& decl, auto const& t)
            {
                if (decl->name == "_" && decl->symbol.expired()) {
                    return;
                }
                auto const symbol = decl->symbol.lock();

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

        if (init->var_decls.size() == 1) {
            auto const& decl = init->var_decls[0];
            if (rhs_exprs.size() == 1) {
                substitute_type(decl, type_of(rhs_exprs[0]));
            } else {
                auto const rhs_type = type::make<type::tuple_type>();
                rhs_type->element_types.reserve(rhs_exprs.size());
                for (auto const& e : rhs_exprs) {
                    rhs_type->element_types.push_back(type_of(e));
                }
                substitute_type(decl, rhs_type);
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

                helper::each([&](auto const& elem_t, auto const& decl)
                                { substitute_type(decl, elem_t); }
                            , rhs_type->element_types
                            , init->var_decls);
            } else {
                if (init->var_decls.size() != rhs_exprs.size()) {
                    semantic_error(init, boost::format("Size of elements in lhs and rhs mismatch\nNote: Size of lhs is %1%, size of rhs is %2%") % init->var_decls.size() % rhs_exprs.size());
                    return;
                }

                helper::each([&](auto const& lhs, auto const& rhs)
                                { substitute_type(lhs, type_of(rhs)); }
                            , init->var_decls
                            , rhs_exprs);
            }
        }
    }

    template<class Walker>
    void visit(ast::node::assignment_stmt const& assign, Walker const& recursive_walker)
    {
        ast::walk_topdown(assign->assignees, var_ref_marker_for_lhs_of_assign{});

        recursive_walker();

        // Note:
        // Do not check assignees' types because of '_' variable

        for (auto const& e : assign->rhs_exprs) {
            if (!type_of(e)) {
                return;
            }
        }

        // Check assignees' immutablity
        // TODO:
        // Use walker
        for (auto const& lhs : assign->assignees) {

            auto const the_var_ref = var_ref_getter_for_lhs_of_assign{}.visit(lhs);
            if (!the_var_ref) {
                semantic_error(assign, "Lhs of assignment must be variable access, index access or member access.");
                return;
            }

            if (the_var_ref->is_ignored_var()) {
                continue;
            } else if (!the_var_ref->type) {
                // Note:
                // Error must occurs
                return;
            }

            assert(!the_var_ref->symbol.expired());

            auto const var_sym = the_var_ref->symbol.lock();
            if (var_sym->immutable) {
                semantic_error(assign, boost::format("Can't assign to immutable variable '%1%'") % var_sym->name);
                return;
            }
        }

        auto const check_types =
            [this, &assign](auto const& t1, auto const& t2)
            {
                if (!t1) {
                    // Note:
                    // When lhs is '_' variable
                    return;
                }

                if (t1 != t2) {
                    semantic_error(
                            assign,
                            boost::format("Types mismatch on assignment\nNote: Type of lhs is '%1%', type of rhs is '%2%'")
                                % type::to_string(t1)
                                % type::to_string(t2)
                        );
                }
            };

        auto const check_type_seqs =
            [&](auto const& type_seq1, auto const& type_seq2)
            {
                helper::each([&](auto const& t1, auto const& t2)
                                { check_types(t1, t2); }
                            , type_seq1, type_seq2);
            };

        auto const to_type_seq = transformed([](auto const& e){ return type::type_of(e); });

        if (assign->assignees.size() == 1) {
            auto const assignee_type = type_of(assign->assignees[0]);
            if (assign->rhs_exprs.size() == 1) {
                auto const rhs_type = type_of(assign->rhs_exprs[0]);
                check_types(assignee_type, rhs_type);
            } else {
                semantic_error(assign, "Assigning multiple values to lhs tuple value is not permitted.");
                return;
            }
        } else {
            if (assign->rhs_exprs.size() == 1) {
                auto const assigner_type = type_of(assign->rhs_exprs[0]);
                auto const maybe_tuple_type = type::get<type::tuple_type>(assigner_type);
                if (!maybe_tuple_type) {
                    semantic_error(assign, boost::format("Assigner's type here must be tuple but actually '%1%'") % assigner_type.to_string());
                    return;
                }

                check_type_seqs(assign->assignees | to_type_seq
                              , (*maybe_tuple_type)->element_types);
            } else {
                if (assign->assignees.size() != assign->rhs_exprs.size()) {
                    semantic_error(assign, boost::format("Size of assignees and assigners mismatches\nNote: Size of assignee is %1%, size of assigner is %2%") % assign->assignees.size() % assign->rhs_exprs.size());
                    return;
                }

                check_type_seqs(assign->assignees | to_type_seq
                              , assign->rhs_exprs | to_type_seq);
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

// TODO:
// Now func main(args) is not permitted and it should be permitted.
// If main is function template and its parameter is 1, the parameter
// should be treated as [string] forcely in symbol_analyzer visitor.
template<class Func>
bool check_main_func(std::vector<Func> const& funcs)
{
    bool found_main = false;

    for (auto const& f : funcs) {
        if (f->name != "main") {
            continue;
        }

        if (found_main) {
            output_semantic_error(
                f->get_ast_node(),
                "Only one main function must exist"
            );
            return false;
        }

        found_main = true;

        if (f->params.empty()) {
            continue;
        }

        if (f->params.size() == 1) {
            auto const& param = f->params[0];
            if (auto const maybe_array = type::get<type::array_type>(param->type)) {
                if ((*maybe_array)->element_type == type::get_builtin_type("string", type::no_opt)) {
                    continue;
                }
            }
        }

        output_semantic_error(
            f->get_ast_node(),
            boost::format("Illegal siganture for main function: '%1%'\nNote: main() or main([string]) is required")
                % f->to_string()
        );
        return false;
    }

    return true;
}

} // namespace detail

void check_semantics(ast::ast &a, scope::scope_tree &t)
{
    detail::symbol_analyzer resolver{t.root, t.root};
    ast::walk_topdown(a.root, resolver);

    if (resolver.failed > 0 || !detail::check_main_func(t.root->functions)) {
        throw semantic_check_error{resolver.failed, "symbol resolution"};
    }
}

} // namespace semantics
} // namespace dachs
