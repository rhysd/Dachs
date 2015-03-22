#include <vector>
#include <string>
#include <cassert>
#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <tuple>
#include <set>
#include <algorithm>

#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/get.hpp>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/algorithm/cxx11/any_of.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm/count_if.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/scope_exit.hpp>

#include "dachs/exception.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/ast/ast_walker.hpp"
#include "dachs/ast/ast_copier.hpp"
#include "dachs/semantics/analyzer.hpp"
#include "dachs/semantics/forward_analyzer_impl.hpp"
#include "dachs/semantics/scope.hpp"
#include "dachs/semantics/symbol.hpp"
#include "dachs/semantics/type.hpp"
#include "dachs/semantics/type_from_ast.hpp"
#include "dachs/semantics/error.hpp"
#include "dachs/semantics/semantics_context.hpp"
#include "dachs/semantics/lambda_capture_resolver.hpp"
#include "dachs/semantics/tmp_member_checker.hpp"
#include "dachs/semantics/tmp_constructor_checker.hpp"
#include "dachs/semantics/const_func_checker.hpp"
#include "dachs/fatal.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"
#include "dachs/helper/each.hpp"
#include "dachs/helper/probable.hpp"

namespace dachs {
namespace semantics {
namespace detail {

using std::size_t;
using helper::variant::get_as;
using helper::variant::has;
using helper::variant::apply_lambda;
using boost::adaptors::transformed;
using boost::adaptors::filtered;
using boost::algorithm::all_of;
using boost::algorithm::any_of;
using type::type_of;

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
    void visit(Node const&, Walker const& w)
    {
        w();
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
    void visit(ast::node::index_access const& a, W const&)
    {
        // Note:
        // {expr}[{expr}] = {rhs}
        a->is_assign = true;
    }

    // Do not mark var ref in UFCS data access
    template<class W>
    void visit(ast::node::ufcs_invocation const& u, W const&)
    {
        // Note:
        // {expr}.name = {rhs}
        u->is_assign = true;
    }

    // Otherwise, simply visit the node
    template<class N, class W>
    void visit(N const&, W const& w)
    {
        w();
    }
};

struct var_ref_getter_for_lhs_of_assign {

    template<class... Args>
    ast::node::var_ref visit(boost::variant<Args...> const& v) const
    {
        return apply_lambda([this](auto const& n){ return visit(n); }, v);
    }

    ast::node::var_ref visit(ast::node::var_ref const& ref) const
    {
        return ref;
    }

    ast::node::var_ref visit(ast::node::index_access const& access) const
    {
        return visit(access->child);
    }

    ast::node::var_ref visit(ast::node::typed_expr const& typed) const
    {
        return visit(typed->child_expr);
    }

    ast::node::var_ref visit(ast::node::ufcs_invocation const& ufcs) const
    {
        return visit(ufcs->child);
    }

    template<class T>
    ast::node::var_ref visit(T const&) const
    {
        return nullptr;
    }
};

template<class Scope, class OuterVisitor>
struct class_template_instantiater : boost::static_visitor<boost::optional<std::string>> {
    Scope const& current_scope;
    OuterVisitor &outer;

    class_template_instantiater(Scope const& s, OuterVisitor &o) noexcept
        : current_scope(s), outer(o)
    {}

    result_type visit(type::type const& t) noexcept
    {
        return t.apply_visitor(*this);
    }

    result_type visit(std::vector<type::type> const& ts) noexcept
    {
        for (auto const& t : ts) {
            if (auto const err = visit(t)) {
                return err;
            }
        }
        return boost::none;
    }

    result_type operator()(type::class_type const& t) noexcept
    {
        if (t->param_types.empty()) {
            return boost::none;
        }

        if (auto const err = visit(t->param_types)) {
            return err;
        }

        if (auto const already_instantiated = current_scope->resolve_class_template(t->name, t->param_types)) {
            t->ref = *already_instantiated;
        } else {
            assert(!t->ref.expired());
            auto const newly_instantiated = outer.instantiate_class_from_specified_param_types(t->ref.lock(), t->param_types);

            if (auto const error = newly_instantiated.get_error()) {
                return *error;
            }

            // Note:
            // Is it OK? Below overwrites a content of type.
            // If it causes an issue, below is alternative.
            //   t = type::make<type::class_type>(boost::get<scope::class_scope>(newly_instantiated))
            t->ref = newly_instantiated.get_unsafe();
            t->param_types.clear();
        }
        t->param_types.clear();

        return boost::none;
    }

    template<class T>
    result_type operator()(T const&) noexcept
    {
        return boost::none;
    }

    result_type operator()(type::tuple_type const& t) noexcept
    {
        return visit(t->element_types);
    }

    result_type operator()(type::func_type const& t) noexcept
    {
        if (auto const err = visit(t->return_type)) {
            return err;
        }
        return visit(t->param_types);
    }

    result_type operator()(type::array_type const& t) noexcept
    {
        return visit(t->element_type);
    }

    result_type operator()(type::pointer_type const& t) noexcept
    {
        return visit(t->pointee_type);
    }

    result_type operator()(type::qualified_type const& t) noexcept
    {
        return visit(t->contained_type);
    }
};

struct self_access_checker {
    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& w)
    {
        if (var->name == "self") {
            is_self_access = true;
        } else {
            w();
        }
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& w)
    {
        if (invocation->args.empty()) {
            w();
            return;
        }

        if (auto const a = get_as<ast::node::var_ref>(invocation->args[0])) {
            if ((*a)->name == "self") {
                is_self_access_with_func = invocation;
            }
            return;
        }

        w();
    }

    template<class Walker>
    void visit(ast::node::ufcs_invocation const& invocation, Walker const& w)
    {
        if (auto const a = get_as<ast::node::var_ref>(invocation->child)) {
            if ((*a)->name == "self") {
                is_self_access_with_ufcs = invocation;
            }
            return;
        }

        w();
    }

    template<class Node, class Walker>
    void visit(Node const&, Walker const& w)
    {
        if (contains_self_access()) {
            return;
        }

        w();
    }

    bool contains_self_access() const noexcept
    {
        return is_self_access
            || static_cast<bool>(is_self_access_with_func)
            || static_cast<bool>(is_self_access_with_ufcs);
    }

    boost::optional<std::string> violated_member_name() const noexcept
    {
        // Note:
        // @foo() -> self.foo() -> foo(self)
        if (is_self_access_with_func) {
            if (auto const callee_var = get_as<ast::node::var_ref>((*is_self_access_with_func)->child)) {
                return (*callee_var)->name;
            }
        }

        // Note:
        // @foo -> self.foo
        if (is_self_access_with_ufcs) {
            return (*is_self_access_with_ufcs)->member_name;
        }

        return boost::none;
    }

    auto func_access_violation() const noexcept
    {
        return static_cast<bool>(is_self_access_with_func);
    }

    auto ufcs_access_violation() const noexcept
    {
        return static_cast<bool>(is_self_access_with_ufcs);
    }

private:
    bool is_self_access = false;
    boost::optional<ast::node::func_invocation> is_self_access_with_func = boost::none;
    boost::optional<ast::node::ufcs_invocation> is_self_access_with_ufcs = boost::none;
};

// Note: Walk to resolve symbol references
class symbol_analyzer {

    scope::any_scope current_scope;
    scope::global_scope const global;
    std::vector<ast::node::lambda_expr> lambdas;
    size_t failed = 0u;
    syntax::importer &importer;
    std::unordered_set<ast::node::function_definition> already_visited_functions;
    std::unordered_set<ast::node::class_definition> already_visited_classes;
    std::unordered_set<ast::node::function_definition> already_visited_ctors;
    boost::optional<scope::func_scope> main_arg_ctor = boost::none;

    using class_instantiation_type_map_type = std::unordered_map<std::string, type::type>;

    // Introduce a new scope and ensure to restore the old scope
    // after the visit process
    template<class Scope, class Walker, class... Args>
    void introduce_scope_and_walk(Scope const& new_scope, Walker const& walker, Args &... args)
    {
        auto const tmp_scope = current_scope;
        current_scope = new_scope;
        walker(args...);
        current_scope = tmp_scope;
    }

    template<class Node, class Message>
    void semantic_error(Node const& n, Message const& msg) noexcept
    {
        output_semantic_error(n, msg);
        failed++;
    }

    template<class Message>
        void semantic_error(std::size_t const l, std::size_t const c, Message const& msg) noexcept
    {
        output_semantic_error(l, c, msg);
        failed++;
    }

    scope::any_scope enclosing_scope_of(scope::any_scope const& scope)
    {
        return apply_lambda(
                    [this](auto const& s) -> scope::any_scope
                    {
                        return enclosing_scope_of(s);
                    }
                    , scope
                );
    }

    template<class Scope>
    scope::any_scope enclosing_scope_of(Scope const& scope)
    {
        return apply_lambda(
                    [](auto const& s) -> scope::any_scope
                    {
                        assert(!s.expired());
                        return s.lock();
                    },
                    scope->enclosing_scope
                );
    }

    template<class FuncDef>
    boost::optional<FuncDef> already_instantiated_func(FuncDef const& def, std::vector<type::type> const& arg_types) const
    {
       auto const scope = def->scope.lock();

       auto const its_the_func
           = [&]
           {
               for (auto const& lr : helper::zipped(scope->params, arg_types)) {
                   auto const& l = boost::get<0>(lr)->type;
                   auto const& r = boost::get<1>(lr);
                   if (l != r) {
                       return false;
                   }
               }
               return true;
           };
       if (its_the_func()) {
           return def;
       } else {
           for (auto const& i : def->instantiated) {
               if (auto const found = already_instantiated_func(i, arg_types)) {
                   return found;
               }
           }
           return boost::none;
       }
    }

    template<class FuncDefNode>
    std::pair<FuncDefNode, scope::func_scope>
    instantiate_function_from_template(
            FuncDefNode const& func_template_def,
            scope::func_scope const& func_template_scope,
            std::vector<type::type> const& arg_types
        )
    {
        assert(func_template_scope->is_template());

        // TODO:
        // If the targe function is already instantiated, return it.

        auto instantiated_func_def = ast::copy_ast(func_template_def);
        auto const enclosing_scope
            = enclosing_scope_of(func_template_scope);

        // Note: No need to check functions duplication
        // Note: Type of parameters are analyzed here
        failed += dispatch_forward_analyzer(instantiated_func_def, enclosing_scope, importer);
        assert(!instantiated_func_def->scope.expired());
        auto instantiated_func_scope = instantiated_func_def->scope.lock();

        // Replace types of params with instantiated types
        assert(instantiated_func_def->params.size() == arg_types.size());

        helper::each(
                [](auto &def_param, auto const& arg, auto &scope_param)
                {
                    if (def_param->type.is_template()) {
                        def_param->type = scope_param->type = arg;
                    } else if (def_param->type.is_class_template()) {
                        def_param->type = scope_param->type = arg;
                    } else if (def_param->type != arg) {
                        // Note:
                        // Never reaches here because type mismatch is already detected in overload resolution
                        DACHS_RAISE_INTERNAL_COMPILATION_ERROR
                    }
                }
                , instantiated_func_def->params
                , arg_types
                , instantiated_func_scope->params
            );

        // Last, symnol analyzer visits
        walk_recursively_with(enclosing_scope, instantiated_func_def); // XXX: Check if it failed

        // Note:
        // Here, instantiated_func_scope->is_template() may return true because partially instantiating
        // constructor is required to determine what class is instantiated from a class template, at getting
        // the information for instantiate class template from constructor.
        //  e.g.
        //      class Foo; a; init(@a); end; end
        //      func main; new Foo{42}; end
        //
        //  In above case, function 'dachs.init(Foo, int)' is instantiated at first to determine the
        //  instantiated class 'Foo(int)'. The function is a template because the first receiver
        //  parameter's type is class template.
        assert((!arg_types.empty() && arg_types[0].is_class_template()) || !instantiated_func_scope->is_template());

        // Add instantiated function to function template node in AST
        func_template_def->instantiated.push_back(instantiated_func_def);

        if (instantiated_func_scope->is_anonymous()) {
            // Note:
            // If the instantiated function is anonymous, it should also be defined in global scope.
            // This is because function scope for lambda is defined in local scope ('unnamed_funcs' member variable).
            // Function scopes in local scope are not target of overload resolution.  So, lambda function scopes are
            // also defined in global scope at visiting ast::node::lambda_expr.
            // (see 'void visit(ast::node::lambda_expr const&, Walker const&)')
            // Instantiated functions from anonymous function templates should also be treated as the same.
            global->define_function(instantiated_func_scope);
        }

        return std::make_pair(instantiated_func_def, instantiated_func_scope);
    }

    type::type from_type_node(ast::node::any_type const& n) noexcept
    {
        return type::from_ast<decltype(*this)>(n, current_scope, *this).apply(
                [](auto const& success){ return success; },
                [&, this](auto const& failure)
                {
                    apply_lambda(
                            [&, this](auto const& t)
                            {
                                semantic_error(t, "  Invalid type '" + failure + "' is specified");
                            }, n
                        );
                    return type::type{};
                }
            );
    }

    std::string make_func_signature(std::string const& name, std::vector<type::type> const& arg_types) const
    {
        return name + '(' + boost::algorithm::join(arg_types | transformed([](auto const& t){ return t.to_string(); }), ",") + ')';
    }

    template<class Node>
    bool /*success?*/ instantiate_all(type::type const& t, Node const& n)
    {
        auto const error = with_current_scope(
            [&t, this](auto const& s)
            {
                class_template_instantiater<decltype(s), decltype(*this)> instantiater{s, *this};
                return t.apply_visitor(instantiater);
            }
        );

        if (error) {
            semantic_error(n, *error);
            semantic_error(n, "  Error occurred while instantiating type '" + t.to_string() + "'");
            return false;
        }

        return true;
    }


    bool already_visited(ast::node::function_definition const& f) const noexcept
    {
        return already_visited_functions.find(f) != std::end(already_visited_functions);
    }

    bool already_visited(ast::node::class_definition const& c) const noexcept
    {
        return already_visited_classes.find(c) != std::end(already_visited_classes);
    }

    bool already_visited_ctor(ast::node::function_definition const& ctor) const noexcept
    {
        return already_visited_ctors.find(ctor) != std::end(already_visited_ctors);
    }

    template<class Location>
    auto generate_default_construct_ast(type::class_type const& t, Location && location)
        -> ast::node::object_construct
    {
        assert(!t->is_template());
        assert(t->param_types.empty());

        auto const ctor_candidates = t->ref.lock()->resolve_ctor({t});
        if (ctor_candidates.size() != 1u) {
            return nullptr;
        }

        auto ctor = *std::begin(ctor_candidates);
        if (ctor->is_template()) {
            std::tie(std::ignore, ctor) = instantiate_function_from_template(ctor->get_ast_node(), ctor, {t});
        }
        assert(!ctor->is_template());

        auto construct
            = helper::make<ast::node::object_construct>(
                    type::to_ast(t, location)
                );
        construct->set_source_location(std::forward<Location>(location));
        construct->type = t;
        construct->constructed_class_scope = t->ref;
        construct->callee_ctor_scope = ctor;

        return construct;
    }

    template<class Predicate>
    auto with_current_scope(Predicate const& p) const
    {
        return apply_lambda(
                    p,
                    current_scope
                );
    }

    boost::optional<scope::func_scope> enclosing_ctor() const
    {
        auto const f = with_current_scope([](auto const& s)
                {
                    return s->get_enclosing_func();
                }
            );

        if (!f || !(*f)->is_ctor()) {
            return boost::none;
        }

        return f;
    }

    // Note:
    // @return : true if the walk was success
    template<class Node>
    bool walk_recursively(Node && node)
    {
        auto const saved_failed = failed;
        ast::walk_topdown(std::forward<Node>(node), *this);
        return failed <= saved_failed;
    }

    template<class Scope, class Node>
    bool walk_recursively_with(Scope const& scope, Node && node)
    {
        auto saved_current_scope = current_scope;
        current_scope = scope;
        auto const result = walk_recursively(std::forward<Node>(node));
        current_scope = std::move(saved_current_scope);
        return result;
    }

    template<class Node>
    bool walk_recursively_with_weak(scope::enclosing_scope_type const& scope, Node && node)
    {
        return apply_lambda(
                [&node, this]
                (auto const& ws)
                {
                    return walk_recursively_with(ws.lock(), std::forward<Node>(node));
                }, scope
            );
    }

public:

    template<class Scope>
    symbol_analyzer(Scope const& root, scope::global_scope const& global, syntax::importer &i) noexcept
        : current_scope{root}, global{global}, importer(i)
    {}

    size_t num_errors() const noexcept
    {
        return failed;
    }

    void analyze_preprocess(scope::func_scope const& main_func)
    {
        if (main_func->params.size() != 1) {
            return;
        }

        auto const error
            = [&main_func, this](auto const& msg)
            {
                semantic_error(main_func->get_ast_node(), msg);
            };

        auto const c = main_func->resolve_class("argv");
        if (!c) {
            error("  Failed to analyze command line argument.  Class 'argv' is not found");
            return;
        }

        auto clazz = *c; // Note: Copy.
        auto const result = visit_class_construct_impl(
                    clazz,
                    std::vector<type::type>{
                        clazz->type,
                        type::get_builtin_type("uint", type::no_opt),
                        type::make<type::pointer_type>(
                                type::make<type::pointer_type>(
                                    type::get_builtin_type("char", type::no_opt)
                                )
                            )
                    }
                );

        if (auto const err = result.get_error()) {
            error(*err);
            error("  Failed to analyze command line argument.  Object construction failed.");
            return;
        }

        scope::func_scope ctor;
        std::tie(std::ignore, ctor) = result.get_unsafe();

        // Note:
        // Preserve the argument to emit main function IR
        main_arg_ctor = std::move(ctor);
    }

    bool analyze_main_func()
    {
        boost::optional<scope::func_scope> found_main = boost::none;

        for (auto const& f : global->functions) {
            if (!f->is_main_func()) {
                continue;
            }

            if (found_main) {
                semantic_error(
                    f->get_ast_node(),
                    boost::format(
                        "  Main function can't be overloaded\n"
                        "  Candidate: %1\n"
                        "  Candidate: %2"
                    ) % (*found_main)->to_string() % f->to_string()
                );
                return false;
            }

            found_main = f;

            if (f->params.empty()) {
                continue;
            }

            if (f->params.size() == 1) {
                auto const& param = f->params[0];

                if (!param->immutable) {
                    semantic_error(f->get_ast_node(), "  Parameter of main function '" + param->name + "' must be immutable");
                    return false;
                }

                if (auto const cls = type::get<type::class_type>(param->type)) {
                    auto const& c = *cls;

                    if (c->name == "argv" && !c->ref.expired()) {
                        continue;
                    }
                }
            }

            semantic_error(
                f->get_ast_node(),
                boost::format(
                    "  Illegal signature for main function: '%1%'\n"
                    "  Note: main() or main(argv) is required"
                    ) % f->to_string()
            );
            return false;
        }

        if (!found_main) {
            semantic_error(
                    1u, 1u,
                    "  Entry point 'main' is not found"
                );
            return false;
        }

        analyze_preprocess(*found_main);

        return true;
    }

    // Push and pop current scope {{{
    template<class Walker>
    void visit(ast::node::statement_block const& block, Walker const& w)
    {
        assert(!block->scope.expired());
        introduce_scope_and_walk(block->scope.lock(), w);
    }

    template<class Walker>
    void visit(ast::node::function_definition const& func, Walker const& w)
    {
        if (already_visited(func)) {
            if (func->ret_type || func->kind == ast::symbol::func_kind::proc || func->is_template()) {
                return;
            }
            recursive_function_return_type_resolver resolver;
            auto func_ = func; // To remove const
            ast::make_walker(resolver).walk(func_);
            if (resolver.result.empty()) {
                semantic_error(func, boost::format("  Can't deduce return type of function '%1%' from return statement") % func->name);
            } else if (boost::algorithm::any_of(resolver.result, [&](auto const& t){ return resolver.result[0] != t; })) {
                std::string note = "";
                for (auto const& t : resolver.result) {
                    note += '\'' + t.to_string() + "' ";
                }
                semantic_error(
                        func,
                        boost::format(
                            "  Conflict among some return types in function '%1%'\n"
                            "  Note: Candidates are: %2%"
                        ) % func->name
                          % note
                    );
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
            semantic_error(func, boost::format("  '?' can't be used in a name of procedure because procedure can't return value"));
            return;
        }

        assert(!func->scope.expired());
        auto scope = func->scope.lock();
        if (scope->is_ctor()) {
            func->ret_type = type::get_unit_type();
            scope->ret_type = *func->ret_type;
        }

        if (scope->is_template()) {
            // XXX:
            // Visit only parameters if function template for overload resolution.
            // This is because type checking and symbol analysis are needed in
            // not function templates but instantiated functions.
            return;
        }

        introduce_scope_and_walk(scope, w);

        // Deduce return type

        return_types_gatherer gatherer;
        auto func_ = func; // to remove const
        ast::make_walker(gatherer).walk(func_);

        if (!gatherer.failed_return_stmts.empty()) {
            semantic_error(
                func,
                boost::format(
                    "  Can't deduce return type of function '%1%' from return statement\n"
                    "  Note: return statement is here: line%2%, col%3%")
                    % func->name
                    % gatherer.failed_return_stmts[0]->line
                    % gatherer.failed_return_stmts[0]->col
            );
            return;
        }

        if (!gatherer.result_types.empty()) {
            if (func->kind == ast::symbol::func_kind::proc) {
                if (gatherer.result_types.size() != 1 || gatherer.result_types[0] != type::get_unit_type()) {
                    semantic_error(func, boost::format("  proc '%1%' can't return any value") % func->name);
                    return;
                }
            }

            if (any_of(
                        gatherer.result_types,
                        [&](auto const& t){ return gatherer.result_types[0] != t; })) {
                semantic_error(
                        func,
                        boost::format("  Mismatch among the result types of return statements in function '%1%'")
                            % func->name
                    );
                return;
            }

            auto const& deduced_type = gatherer.result_types[0];

            if (func->ret_type) {
                auto const& ret = *func->ret_type;

                if (ret.is_template()){
                    if (!deduced_type.is_instantiated_from(ret)) {
                        semantic_error(
                                func,
                                boost::format(
                                    "  Return template type of function '%1%' can't instantiate deduced type '%3%'\n"
                                    "  Note: Specified template is '%2%'\n"
                                    "  Note: Deduced type is '%3%'"
                                ) % func->name
                                % ret.to_string()
                                % deduced_type.to_string()
                            );
                        return;
                    }
                } else if (ret != deduced_type) {
                    semantic_error(
                            func,
                            boost::format(
                                "  Return type of function '%1%' mismatch\n"
                                "  Note: Specified type is '%2%'\n"
                                "  Note: Deduced type is '%3%'"
                            ) % func->name
                              % ret.to_string()
                              % deduced_type.to_string()
                        );
                    return;
                }
            }

            func->ret_type = deduced_type;
            scope->ret_type = func->ret_type;
        } else {
            if (func->ret_type && *func->ret_type != type::get_unit_type()) {
                semantic_error(
                        func,
                        boost::format(
                            "  Return type of function '%1%' mismatch\n"
                            "  Note: Specified type is '%2%'\n"
                            "  Note: Deduced type is '%3%'"
                        ) % func->name
                          % func->ret_type->to_string()
                          % type::get_unit_type()->to_string()
                    );
            }
            func->ret_type = type::get_unit_type();
            scope->ret_type = *func->ret_type;
        }

        if (is_query_function && *func->ret_type != type::get_builtin_type("bool", type::no_opt)) {
            semantic_error(func, boost::format("  Function '%1%' must return bool because it includes '?' in its name") % func->name);
        }

        const_member_func_checker const_checker{scope, func_};
        scope->is_const_ = const_checker.check_const();
    }
    // }}}

    template<class Walker>
    void visit(ast::node::variable_decl const& decl, Walker const& w)
    {
        auto const maybe_ctor = enclosing_ctor();

        auto const visit_decl =
            [this, &decl, &maybe_ctor](auto &scope)
            {
                if (decl->name == "_") {
                    return true;
                }

                if (decl->is_instance_var()) {
                    if (!maybe_ctor) {
                        semantic_error(
                                    decl,
                                    "  Instance variable '" + decl->name + "' can be initialized only in constructor"
                                );
                        return false;
                    }

                    auto const& ctor = *maybe_ctor;
                    assert((ctor->params[0]->name == "self") && type::is_a<type::class_type>(ctor->params[0]->type));
                    auto const receiver = *type::get<type::class_type>(ctor->params[0]->type);
                    auto const receiver_scope = receiver->ref.lock();
                    if (!receiver_scope->resolve_instance_var(decl->name.substr(1u))) {
                        semantic_error(
                                    decl,
                                    "  Instance variable '" + decl->name + "' is not defined in class '" + receiver_scope->to_string() + "'"
                                );
                        return false;
                    }
                }

                auto new_var = symbol::make<symbol::var_symbol>(decl, decl->name, !decl->is_var);
                decl->symbol = new_var;

                // Set type if the type of variable is specified
                if (decl->maybe_type) {
                    new_var->type
                        = from_type_node(*decl->maybe_type);
                    if (!new_var->type) {
                        return false;
                    }
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
        } else if (auto maybe_class = get_as<scope::class_scope>(current_scope)) {
            // Note:
            // Do nothing because they are already analyzed in forward_analyzer.
        } else {
            DACHS_RAISE_INTERNAL_COMPILATION_ERROR
        }

        w();
    }

    template<class Walker>
    void visit(ast::node::var_ref const& var, Walker const& w)
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

        auto const maybe_var_symbol = boost::apply_visitor(scope::var_symbol_resolver{name}, current_scope);
        if (!maybe_var_symbol) {
            semantic_error(var, boost::format("  Symbol '%1%' is not found") % name);
            return;
        }

        auto const& sym = *maybe_var_symbol;
        var->symbol = sym;

        auto const should_copy_deeply = [](auto const& s) -> bool
        {
            if (s->is_builtin) {
                // Note:
                // If built-in
                return true;
            }

            // TODO:
            // If the function is not a template, deep copy is not needed

            return ast::node::is_a<ast::node::function_definition>(s->ast_node);
        }(sym);

        if (should_copy_deeply) {
            auto const g = type::get<type::generic_func_type>(sym->type);
            assert(g);
            assert((*g)->ref);
            assert(!(*g)->ref->expired());

            // XXX:
            // Too ad hoc!
            // Allocate new type object not to affect the original type object
            // when the new type object is modified.

            var->type = type::make<type::generic_func_type>((*g)->ref);
        } else {
            var->type = sym->type;
        }

        w();
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

    void visit_array_literal_construction(ast::node::array_literal const& arr_lit, type::type const& elem_type)
    {
        auto cls = with_current_scope([](auto const& s){ return s->resolve_class("array"); });
        assert(cls);
        auto &array_class = *cls;
        assert(array_class->is_template());

        auto const result = visit_class_construct_impl(
                array_class,
                std::vector<type::type>{
                    array_class->type,
                    type::make<type::pointer_type>(elem_type),
                    type::get_builtin_type("uint", type::no_opt)
                }
            );

        if (auto const error = result.get_error()) {
            semantic_error(arr_lit, *error);
            semantic_error(arr_lit, "  Error while analyzing array literal");
            return;
        }

        auto const class_and_ctor = result.get_unsafe();
        arr_lit->constructed_class_scope = class_and_ctor.first;
        arr_lit->callee_ctor_scope = class_and_ctor.second;
        arr_lit->type = class_and_ctor.first->type;
    }

    template<class Walker>
    void visit(ast::node::array_literal const& arr_lit, Walker const& w)
    {
        w();

        // Note: Check only the head of element because Dachs doesn't allow implicit type conversion
        if (arr_lit->element_exprs.empty()) {
            if (!arr_lit->type) {
                semantic_error(arr_lit, "  Empty array must be typed by ':'");
                return;
            }

            if (auto const p = arr_lit->type.get_array_underlying_type()) {
                visit_array_literal_construction(arr_lit, (*p)->pointee_type);
                return;
            } else {
                semantic_error(arr_lit, "  Invalid type '" + arr_lit->type.to_string() + "' is specified for array literal");
                return;
            }
        }

        auto arg0_type = type_of(arr_lit->element_exprs[0]);
        if (any_of(
                arr_lit->element_exprs,
                [&](auto const& e){ return arg0_type != type_of(e); })
            ) {
            semantic_error(arr_lit, boost::format("  All types of array elements must be '%1%'") % arg0_type.to_string());
            return;
        }

        if (auto const& arg0_arr_type = type::get<type::array_type>(arg0_type)) {
            auto const knows_size =
                [](auto const& e) -> bool { return *(*type::get<type::array_type>(type_of(e)))->size; };

            if (all_of(arr_lit->element_exprs, knows_size)) {
                auto const arg0_size = *(*arg0_arr_type)->size;
                if (any_of(
                        arr_lit->element_exprs,
                        [arg0_size](auto const& e){ return *(*type::get<type::array_type>(type_of(e)))->size != arg0_size; }
                    )) {
                    semantic_error(arr_lit, "  All elements in array must be the same length");
                    return;
                }

            } else if (any_of(
                    arr_lit->element_exprs,
                    knows_size
                )) {
                // Note:
                // Some elements' types don't have its length and at least one element's type has its length
                semantic_error(arr_lit, "  Some array elements have fixed length and others don't");
                return;
            }
        }

        visit_array_literal_construction(arr_lit, arg0_type);
    }

    template<class Walker>
    void visit(ast::node::string_literal const& str_lit, Walker const&)
    {
        auto c = with_current_scope([](auto const& s){ return s->resolve_class("string"); });
        assert(c);
        auto &str_scope = *c;

        auto const result = visit_class_construct_impl(
                    str_scope,
                    std::vector<type::type>{
                        str_scope->type,
                        type::make<type::pointer_type>(type::get_builtin_type("char", type::no_opt)),
                        type::get_builtin_type("uint", type::no_opt)
                    }
                );

        if (auto const error = result.get_error()) {
            semantic_error(str_lit, *error);
            semantic_error(str_lit, "  Error while analyzing string literal");
            return;
        }

        auto const class_and_ctor = result.get_unsafe();
        str_lit->constructed_class_scope = class_and_ctor.first;
        str_lit->callee_ctor_scope = class_and_ctor.second;
        str_lit->type = class_and_ctor.first->type;
        assert(str_lit->type.is_string_class());
    }

    template<class Walker>
    void visit(ast::node::tuple_literal const& tuple_lit, Walker const& w)
    {
        if (tuple_lit->element_exprs.size() == 1) {
            semantic_error(tuple_lit, "  Size of tuple should not be 1");
        }
        w();
        auto const type = type::make<type::tuple_type>();
        type->element_types.reserve(tuple_lit->element_exprs.size());
        for (auto const& e : tuple_lit->element_exprs) {
            type->element_types.push_back(type_of(e));
        }
        tuple_lit->type = type;
    }

    template<class Walker>
    void visit(ast::node::dict_literal const&, Walker const&)
    {
        throw not_implemented_error{__FILE__, __func__, __LINE__, "dictionary literal"};
    }

    template<class Walker>
    void visit(ast::node::lambda_expr const& lambda, Walker const&)
    {
        walk_recursively(lambda->def);

        assert(!lambda->def->scope.expired());
        auto const lambda_scope = lambda->def->scope.lock();
        assert(lambda_scope->is_anonymous());

        global->define_function(lambda_scope);
        lambda->type = type::make<type::generic_func_type>(lambda_scope);
        lambdas.push_back(lambda);
    }

    template<class Outer>
    struct index_access_analyzer : boost::static_visitor<boost::optional<std::string>> {
        ast::node::index_access const& access;
        type::type const& index_type;
        Outer &outer;

        explicit index_access_analyzer(ast::node::index_access const& a, type::type const& i, Outer &o) noexcept
            : access(a), index_type(i), outer(o)
        {}

        template<class T>
        std::string not_found(T const& t) const
        {
            return "  Index access operator '[]' for '" + t->to_string() + "' indexed by '" + index_type.to_string() + "' is not found";
        }

        result_type operator()(type::array_type const& arr) const
        {
            if (!index_type.is_builtin("int") && !index_type.is_builtin("uint")) {
                return "  Index of array must be int or uint but actually '" + index_type.to_string() + "'";
            }
            access->type = arr->element_type;
            return boost::none;
        }

        result_type operator()(type::pointer_type const& ptr) const
        {
            if (!index_type.is_builtin("int") && !index_type.is_builtin("uint")) {
                return "  Index of pointer must be int or uint but actually '" + index_type.to_string() + "'";
            }
            access->type = ptr->pointee_type;
            return boost::none;
        }

        result_type operator()(type::tuple_type const& tpl) const
        {
            auto const maybe_primary_literal = get_as<ast::node::primary_literal>(access->index_expr);
            if (!maybe_primary_literal ||
                    (!has<int>((*maybe_primary_literal)->value)
                     && !has<uint>((*maybe_primary_literal)->value))
               ) {
                return std::string{"  Index of tuple must be int or uint literal"};
            }

            auto const& literal = (*maybe_primary_literal)->value;
            if (auto const maybe_int_lit = get_as<int>(literal)) {
                auto const idx = *maybe_int_lit;
                if (idx < 0 || static_cast<unsigned int>(idx) >= tpl->element_types.size()) {
                    return "  Index access is out of bounds\n  Note: Index is " + std::to_string(idx);
                }
                access->type = tpl->element_types[idx];
            } else if (auto const maybe_uint_lit = get_as<unsigned int>(literal)) {
                auto const idx = *maybe_uint_lit;
                if (idx >= tpl->element_types.size()) {
                    return "  Index access is out of bounds\n  Note: Index is " + std::to_string(idx);
                }
                access->type = tpl->element_types[idx];
            } else {
                DACHS_RAISE_INTERNAL_COMPILATION_ERROR
            }

            return boost::none;
        }

        result_type operator()(type::builtin_type const& builtin) const
        {
            if (builtin->name == "string") {
                if (!index_type.is_builtin("int") && !index_type.is_builtin("uint")) {
                    return "  Index of string must be int or uint but actually '" + index_type.to_string() + "'";
                }
                access->type = type::get_builtin_type("char", type::no_opt);
                return boost::none;
            } else {
                return not_found(builtin);
            }
        }

        template<class Types>
        result_type analyze_index_operator(std::string const& op, Types const& arg_types) const
        {
            if (auto const error = outer.visit_invocation(
                        access,
                        op,
                        arg_types
                    )
            ) {
                return error;
            }

            if (auto const violated = is_const_violated_invocation(access)) {
                return "  Member function '" + access->callee_scope.lock()->to_string()
                     + "' modifies member(s) of immutable object '" + (*violated)->name + "'\n"
                       "  Note: this function is called for index access operator '" + op + "'";
            }

            assert(!access->callee_scope.expired());
            auto const callee = access->callee_scope.lock();
            if (callee->ret_type) {
                access->type = *callee->ret_type;
            }

            return boost::none;
        }

        template<class Type>
        result_type operator()(Type const& t) const
        {
            if (access->is_assign) {
                // Note:
                // Skip analysis when the index access is at lhs of assignment.
                // The analysis for index access at lhs of assignment will is done at
                // visiting ast::node::assignment_stmt.
                return boost::none;
            }

            return analyze_index_operator(
                    "[]",
                    std::vector<type::type>{
                        t, index_type
                    }
                );
        }


        // Note:
        // I defined this function here because below is an edge case of
        // index access.  I wanted to gather functions for index access here.
        result_type analyze_lhs_of_assign(type::type const& rhs_type) const
        {
            if (access->type || !rhs_type) {
                // Note:
                // When the type of index access is already determined,
                // it means that it doesn't need function call and already analyzed
                // at visiting ast::node::index_access.
                return boost::none;
            }

            return analyze_index_operator(
                    "[]=",
                    std::vector<type::type>{
                        type_of(access->child), index_type, rhs_type
                    }
                );
        }
    };

    template<class Walker>
    void visit(ast::node::index_access const& access, Walker const& w)
    {
        w();

        auto const child_type = type_of(access->child);
        auto const index_type = type_of(access->index_expr);

        if (!child_type || !index_type) {
            return;
        }

        index_access_analyzer<symbol_analyzer> analyzer{access, index_type, *this};
        auto const error = child_type.apply_visitor(analyzer);
        if (error) {
            semantic_error(access, *error);
        }
    }

    template<class Node>
    type::type visit_builtin_binary_expr(Node const& node, std::string const& op, type::type const& lhs_type, type::type const& rhs_type)
    {
        if (lhs_type != rhs_type) {
            semantic_error(
                    node,
                    boost::format(
                        "  Type mismatch in built-in binary operator '%1%'\n"
                        "  Note: Type of lhs is %2%\n"
                        "  Note: Type of rhs is %3%\n"
                        "  Note: User-defined operators for builtin-types are not permitted"
                    ) % op
                      % lhs_type.to_string()
                      % rhs_type.to_string()
                    );
            return {};
        }

        if (helper::any_of({"==", "!=", ">", "<", ">=", "<="}, op)) {
            return type::get_builtin_type("bool", type::no_opt);
        } else if (op == "&&" || op == "||") {
            if (lhs_type != type::get_builtin_type("bool", type::no_opt)) {
                semantic_error(
                        node,
                        boost::format(
                            "  Operator '%1%' for type '%2%' is found\n"
                            "  Note: User-defined operators for builtin-types are not permitted"
                        ) % op % lhs_type.to_string()
                    );
            }
            return type::get_builtin_type("bool", type::no_opt);
        } else {
            return lhs_type;
        }
    }

    void visit_binary_expr_call(ast::node::binary_expr const& bin_expr, type::type const& lhs_type, type::type const& rhs_type)
    {
        auto const error = visit_invocation(
                bin_expr,
                bin_expr->op,
                std::vector<type::type>{
                    lhs_type, rhs_type
                }
            );

        if (error) {
            semantic_error(bin_expr, *error);
            return;
        }

        if (auto const violated = is_const_violated_invocation(bin_expr)) {
            semantic_error(
                    bin_expr,
                    boost::format(
                        "  Member function '%1%' modifies member(s) of immutable object '%2%'\n"
                        "  Note: this function is called for binary operator '%3%'"
                    ) % bin_expr->callee_scope.lock()->to_string() % (*violated)->name % bin_expr->op
                );
        }
    }

    template<class Walker>
    void visit(ast::node::binary_expr const& bin_expr, Walker const& w)
    {
        w();

        auto const lhs_type = type_of(bin_expr->lhs);
        auto const rhs_type = type_of(bin_expr->rhs);

        if (!lhs_type || !rhs_type) {
            return;
        }

        if (lhs_type.is_builtin() && rhs_type.is_builtin()) {
            bin_expr->type = visit_builtin_binary_expr(bin_expr, bin_expr->op, lhs_type, rhs_type);
            return;
        }

        visit_binary_expr_call(bin_expr, lhs_type, rhs_type);
    }

    void visit_builtin_unary_expr(ast::node::unary_expr const& unary, type::type const& operand_type)
    {
        if (unary->op == "!") {
            if (operand_type != type::get_builtin_type("bool", type::no_opt)) {
                semantic_error(
                        unary,
                        boost::format(
                            "  Operator '%1%' for type '%2%' is found\n"
                            "  Note: User-defined operators for builtin-types are not permitted"
                        ) % unary->op % operand_type.to_string()
                    );
            }
            unary->type = type::get_builtin_type("bool", type::no_opt);
        } else {
            unary->type = operand_type;
        }
    }

    void visit_unary_expr_call(ast::node::unary_expr const& unary, type::type const& operand_type)
    {
        auto const error = visit_invocation(
                    unary,
                    unary->op,
                    std::vector<type::type>{ operand_type }
                );

        if (error) {
            semantic_error(unary, *error);
            return;
        }

        if (auto const violated = is_const_violated_invocation(unary)) {
            semantic_error(
                    unary,
                    boost::format(
                        "  Member function '%1%' modifies member(s) of immutable object '%2%'\n"
                        "  Note: this function is called for unary operator '%3%'"
                    ) % unary->callee_scope.lock()->to_string() % (*violated)->name % unary->op
                );
        }
    }

    template<class Walker>
    void visit(ast::node::unary_expr const& unary, Walker const& w)
    {
        w();

        auto const operand_type = type_of(unary->expr);

        if (!operand_type) {
            return;
        }

        if (operand_type.is_builtin()) {
            visit_builtin_unary_expr(unary, operand_type);
            return;
        }

        visit_unary_expr_call(unary, operand_type);
    }

    template<class Walker>
    void visit(ast::node::if_expr const& if_, Walker const& w)
    {
        w();

        auto const condition_type = type_of(if_->condition_expr);
        auto const then_type = type_of(if_->then_expr);
        auto const else_type = type_of(if_->else_expr);

        if (!condition_type || !then_type || !else_type) {
            return;
        }

        if (condition_type != type::get_builtin_type("bool", type::no_opt)) {
            semantic_error(
                    if_,
                    boost::format(
                        "  Type of condition in if expression must be bool\n"
                        "  Note: Type of condition is '%1%'"
                    ) % condition_type.to_string()
                );
            return;
        }

        if (then_type != else_type) {
            semantic_error(
                    if_,
                    boost::format(
                        "  Type mismatch between type of then clause and else clause\n"
                        "  Note: Type of then clause is '%1%'\n"
                        "  Note: Type of else clause is '%2%'"
                    ) % then_type.to_string() % else_type.to_string()
                );
            return;
        }

        if_->type = then_type;
    }

    template<class ArgTypes>
    helper::probable<scope::func_scope>
    resolve_func_call(std::string const& func_name, ArgTypes const& arg_types)
    {
        // Note:
        // generic_func_type::ref is not available because it is not updated
        // even if overload functions are resolved.
        auto const func_candidates =
            with_current_scope(
                    [&](auto const& s)
                    {
                        return s->resolve_func(func_name, arg_types);
                    }
                );

        if (func_candidates.empty()) {
            return helper::oops_fmt("  Function for '%1%' is not found", make_func_signature(func_name, arg_types));
        } else if (func_candidates.size() > 1u) {
            std::string errmsg = "  Function candidates for '" + make_func_signature(func_name, arg_types) + "' are ambiguous";
            for (auto const& c : func_candidates) {
                errmsg += "\n  Candidate: " + c->to_string();
            }
            return helper::oops(errmsg);
        }

        auto func = *std::begin(std::move(func_candidates));

        if (func->is_builtin) {
            assert(func->ret_type);
            return {func};
        }

        auto func_def = func->get_ast_node();

        if (func->is_template()) {
            std::tie(func_def, func) = instantiate_function_from_template(func_def, func, arg_types);
            assert(!global->ast_root.expired());
        }

        if (!func_def->ret_type) {
            // Note:
            // enclosing scope of function scope is always global scope
            if (!walk_recursively_with(global, func_def)) {
                return helper::oops_fmt(
                        "  Failed to analyze function '%1%' defined at line:%2%, col:%3%"
                     , func->to_string(), func_def->line, func_def->col
                );
            }
        }

        if (!func->ret_type) {
            return helper::oops_fmt("  Cannot deduce the return type of function '%1%'", func->to_string());
        }

        // Note:
        // Check function accessibility
        if (!func_def->is_public()) {
            auto const f = with_current_scope([](auto const& s)
                    {
                        return s->get_enclosing_func();
                    }
                );
            auto const t = type::get<type::class_type>(func->params[0]->type);

            assert(f);
            assert(t);

            auto const error
                = [t=*t, &func, this]
                {
                    return helper::oops_fmt("  member function '%1%' is a private member of class '%2%'"
                                , func->to_string(), t->name
                            );
                };

            if (auto const c = (*f)->get_receiver_class_scope()) {
                if ((*t)->name != (*c)->name) {
                    return error();
                }
            } else {
                return error();
            }
        }

        return {func};
    }

    template<class Node, class ArgTypes>
    boost::optional<std::string> visit_invocation(Node const& node, std::string const& func_name, ArgTypes const& arg_types)
    {
        auto const resolved = resolve_func_call(func_name, arg_types);
        if (auto const error =resolved.get_error()) {
            return *error;
        }

        auto const func = resolved.get_unsafe();

        node->type = *func->ret_type;
        node->callee_scope = func;

        return boost::none;
    }

    // Note:
    // When 'foo.bar(...)' invocation, check if object 'foo' has an instance variable 'bar',
    // then when 'bar' is callable, calling (foo.bar)(...) has higher priority than 'bar(foo, ...)'
    // UFCS invocation.
    //
    // @return: success?
    void visit_instance_var_invocation(ast::node::func_invocation const& invocation)
    {
        assert(invocation->is_ufcs);
        assert(invocation->args.size() > 0u);

        auto const var_ref = get_as<ast::node::var_ref>(invocation->child);
        if (!var_ref) {
            return;
        }
        auto const& name = (*var_ref)->name;

        auto const receiver_type = type::get<type::class_type>(type::type_of(invocation->args[0]));
        if (!receiver_type) {
            return;
        }

        auto const receiver_class = (*receiver_type)->ref.lock();

        auto const instance_var = helper::find_if(
                receiver_class->instance_var_symbols,
                [&name](auto const& s){ return s->name == name; }
            );
        if (!instance_var) {
            return;
        }

        if (!type::is_a<type::generic_func_type>((*instance_var)->type)) {
            return;
        }

        // bar(foo, ...)
        // foo.bar(...)

        auto const instance_var_access = helper::make<ast::node::ufcs_invocation>(invocation->args[0], name);
        instance_var_access->set_source_location(*invocation);
        instance_var_access->type = (*instance_var)->type;

        invocation->args.erase(std::begin(invocation->args));
        invocation->child = instance_var_access;
    }

    // TODO:
    // If return type of function is not determined, interrupt current parsing and parse the
    // function at first.  Add the function to the list which are already visited and check it at
    // function_definition node
    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& w)
    {
        w();

        if (invocation->is_ufcs) {
            visit_instance_var_invocation(invocation);
        }

        auto const child_type = type::type_of(invocation->child);
        if (!child_type) {
            return;
        }

        auto const maybe_func_type = type::get<type::generic_func_type>(child_type);
        if (!maybe_func_type) {
            semantic_error(
                    invocation,
                    "  Only function type can be called\n"
                    "  Note: The type '" + child_type.to_string() + "' is not a function"
                );
            return;
        }

        auto const& func_type = *maybe_func_type;

        if (!func_type->ref) {
            semantic_error(
                    invocation,
                    "  " + func_type->to_string() + " is an invalid function reference"
                );
            return;
        }
        assert(!func_type->ref->expired());

        std::vector<type::type> arg_types;
        arg_types.reserve(invocation->args.size() + 1); // +1 for do block
        // Get type list of arguments
        boost::transform(invocation->args, std::back_inserter(arg_types), [](auto const& e){ return type_of(e);});

        for (auto const& arg_type : arg_types) {
            if (!arg_type) {
                return;
            }
        }

        auto callee_scope = func_type->ref->lock();
        auto const error = visit_invocation(invocation, callee_scope->name, arg_types);
        if (error) {
            semantic_error(invocation, *error);
            return;
        }

        if (callee_scope->is_template()) {
            // Note:
            // Replace function template with instantiated function within the newly generated generic
            // function type.  This is because lambda captures are associated with the instantiated function.
            // Below makes code generation find its lambda captures properly.
            func_type->ref = invocation->callee_scope;
        }

        if (auto const violated = is_const_violated_invocation(invocation)) {
            semantic_error(
                    invocation,
                    boost::format("  Member function '%1%' modifies member(s) of immutable object '%2%'")
                        % invocation->callee_scope.lock()->to_string() % (*violated)->name
                );
        }

        if (callee_scope->is_main_func()) {
            semantic_error(invocation, "  You can't invoke 'main' function");
        }
    }

    template<class Walker>
    void visit(ast::node::typed_expr const& typed, Walker const& w)
    {
        auto const specified_type = from_type_node(typed->specified_type);
        if (!specified_type) {
            return;
        }

        if (specified_type.is_array_class()) {
            // Note:
            // Edge case for empty array literals
            apply_lambda(
                [&](auto const& node)
                {
                    node->type = specified_type;
                }, typed->child_expr
            );
        } // else if (auto const maybe_child_dict = ...) {

        w();

        auto const actual_type = type_of(typed->child_expr);

        // Note:
        // Check the class is an instance of a specified class template
        //   e.g.
        //     class X; a; init; end; end
        //     x := new X(int)
        //
        //     # OK: Type of 'x' is 'X(int)' and it is an instance of 'X(T)'
        //     x : X
        //

        if (actual_type.is_instantiated_from(specified_type)) {
            typed->type = actual_type;
            return;
        }

        // Note:
        // This is workaround related to the difference of size of array.
        // User doesn't specify the length of array and any length array should be accepted
        // as actual type.  However, type::array_type::operator== checks equality strictly.
        // So ignore the length on comparing here.
        if (auto const s = type::get<type::array_type>(specified_type)) {
            if (auto const a = type::get<type::array_type>(actual_type)) {
                if ((*s)->element_type == (*a)->element_type) {
                    typed->type = actual_type;
                    return;
                }
            }
        }
        if (auto const s = specified_type.get_array_underlying_type()) {
            if (auto const a = actual_type.get_array_underlying_type()) {
                if ((*s)->pointee_type == (*a)->pointee_type) {
                    typed->type = actual_type;
                    return;
                }
            }
        }


        if (actual_type != specified_type) {
            semantic_error(typed, boost::format("  Types mismatch; specified '%1%' but actually typed to '%2%'") % specified_type.to_string() % actual_type.to_string());
            return;
        }

        typed->type = actual_type;
    }

    template<class Walker>
    void visit(ast::node::cast_expr const& cast, Walker const& w)
    {
        w();
        cast->type = from_type_node(cast->cast_type);

        // TODO:
        // Find cast function and get its result type
    }

    // Note:
    // 'class_name' is a name of class which the variable belongs to
    bool is_accessible_instance_var(scope::class_scope const& clazz, symbol::var_symbol const& var) const
    {
        if (var->is_public) {
            return true;
        }

        auto const f = with_current_scope([](auto const& s)
                {
                    return s->get_enclosing_func();
                }
            );

        assert(f);

        if (auto const c = (*f)->get_receiver_class_scope()) {
            if ((*c)->name == clazz->name) {
                // Note:
                // If the instance variable is a private member,
                // it can only be accessed in the class.
                return true;
            }
        }

        return false;
    }

    template<class Walker>
    void visit(ast::node::ufcs_invocation const& ufcs, Walker const& w)
    {
        w();

        auto const child_type = type_of(ufcs->child);
        if (!child_type) {
            return;
        }

        bool private_member_access_was_rejected = false;
        if (auto const clazz = type::get<type::class_type>(child_type)) {
            assert(!(*clazz)->ref.expired());
            auto const scope = (*clazz)->ref.lock();

            if (auto const instance_var = scope->resolve_instance_var(ufcs->member_name)) {
                if (is_accessible_instance_var(scope, *instance_var)) {
                    ufcs->type = (*instance_var)->type;
                    return;
                } else {
                    private_member_access_was_rejected = true;
                }
            }
        }

        // Check data member 'ufcs->member_name' of 'ufcs->child'.
        // Now, built-in data member is only available.
        auto const checked = check_member_var(ufcs, child_type, current_scope);
        if (auto const error = checked.get_error()) {
            semantic_error(ufcs, *error);
            return;
        } else {
            auto const t = checked.get_unsafe();
            if (t) {
                ufcs->type = t;
                return;
            }
        }

        if (ufcs->is_assign) {
            semantic_error(
                    ufcs,
                    boost::format("  No member named '%1%' is found for assignment in '%2%'")
                        % ufcs->member_name % child_type.to_string()
                );
            return;
        }

        // Note:
        // Check function call.  'a.foo' means 'foo(a)'
        auto const error = visit_invocation(ufcs, ufcs->member_name, std::vector<type::type>{{child_type}});
        if (error) {
            semantic_error(ufcs, *error);
            if (private_member_access_was_rejected) {
                // XXX:
                // This should be a note for the error message.
                semantic_error(
                        ufcs,
                        "  Did you mean the access to instance variable '"
                            + ufcs->member_name
                            + "'? It is not accessible because of a private member"
                    );
            }
            return;
        }

        if (auto const violated = is_const_violated_invocation(ufcs)) {
            semantic_error(
                    ufcs,
                    boost::format("  Member function '%1%' modifies member(s) of immutable object '%2%'")
                        % ufcs->callee_scope.lock()->to_string() % (*violated)->name
                    );
        }
    }

    auto generate_instantiation_map(scope::class_scope const& scope) const
    {
        std::unordered_map<std::string, type::type> map;
        for (auto const& s : scope->instance_var_symbols) {
            assert(s->type);
            map[s->name] = type::is_a<type::template_type>(s->type) ? type::type{} : s->type;
        }

        return map;
    }

    // TODO:
    // This function is too long.  It should be refactored.
    template<class Map>
    boost::optional<Map> check_template_instantiation_with_ctor(Map &&map, scope::func_scope const& ctor, ast::node::function_definition const& ctor_def)
    {
        assert(ctor->is_ctor());
        std::unordered_set<std::string> initialized_names;

        auto const check_instance_var_type
            = [&, this](auto const& var_node, auto const& var_sym)
            {
                std::string name = var_sym->name.substr(1u)/* omit '@' */;
                auto const i = map.find(name);
                assert(i != std::end(map));
                auto &var_type = i->second;

                auto const error
                    = [&, this]
                    {
                        semantic_error(
                                var_node, boost::format(
                                    "  Type of instance variable '%1%' mismatches.\n"
                                    "  Note: Tried to substitute type '%2%' but it was actually type '%3%'"
                                ) % var_sym->name % var_type.to_string() % var_sym->type.to_string()
                            );
                    };

                if (!var_type || var_sym->type.is_instantiated_from(var_type)) {
                    var_type = var_sym->type;
                } else if (var_type != var_sym->type) {
                    error();
                    return false;
                }

                initialized_names.emplace(std::move(name));
                return true;
            };

        auto const is_initialized
            = [&](auto const& name)
            {
                return initialized_names.find(name) != std::end(initialized_names);
            };

        std::unordered_set<ast::node::parameter> checked;
        bool fail = false;

        auto const all_initialized
            = [&]
            {
                return map.size() == initialized_names.size();
            };

        auto &body_stmts = ctor_def->body->value;

        // Note:
        // Parameter types are already checked in forward analyzer
        for (auto const& p : ctor_def->params) {
            if (p->is_instance_var_init()) {
                if (!check_instance_var_type(p, p->param_symbol.lock())) {
                    fail = true;
                }
                checked.insert(p);
            }
        }

        auto const init_end_point_idx
            = [&body_stmts]() -> std::size_t
            {
                for (auto const idx : helper::rindices(body_stmts)) {
                    auto const& stmt = body_stmts[idx];
                    if (auto const& init = get_as<ast::node::initialize_stmt>(stmt)) {
                        if (any_of((*init)->var_decls, [](auto const& d){ return d->is_instance_var(); })) {
                            return idx + 1;
                        }
                    }
                }
                return 0;
            }();

        for (auto const idx : helper::indices(body_stmts)) {
            auto &stmt = body_stmts[idx];
            if (idx < init_end_point_idx && !all_initialized()) {
                self_access_checker checker;
                ast::walk_topdown(stmt, checker);
                if (checker.contains_self_access()) {
                    if (auto const n = checker.violated_member_name()) {
                        if (!is_initialized(*n)) {
                            semantic_error(
                                    ast::node::location_of(stmt),
                                    checker.func_access_violation()
                                        ? "  Calling member function '" + *n
                                            + "' is not permitted until all instance variables are initialized in constructor"
                                        : "  Access to instance variable '" + *n + "' here is not permitted in constructor because '"
                                            + *n + "' may not be initialized here yet"
                                );
                            fail = true;
                        }
                    } else {
                        semantic_error(
                                ast::node::location_of(stmt),
                                "  Access to 'self' is not permitted until all instance variables are initialized in constructor"
                            );
                        fail = true;
                    }
                }
            }

            if (auto const init = get_as<ast::node::initialize_stmt>(const_cast<ast::node::compound_stmt const&>(stmt))) {
                for (auto const& decl : (*init)->var_decls) {
                    if (decl->is_instance_var()) {
                        auto const dup = helper::find_if(checked, [&n = decl->name](auto const& d){ return d->name == n; });
                        if (dup) {
                            // Note:
                            // This check is necessary because the duplication of symbol names
                            // between parameter and initialize statement doesn't occur an error
                            // though a warning is raised.
                            semantic_error(
                                    decl,
                                    boost::format(
                                        "  Instance variable '%1%' is initialized twice or more times.\n"
                                        "  Note: Instance variable can only be initialized once.\n"
                                        "  Note: First initialization is at line:%2%, col:%3%."
                                    ) % decl->name % (*dup)->line % (*dup)->col
                                );
                            fail = true;
                        } else if (decl->symbol.expired()
                                || !check_instance_var_type(decl, decl->symbol.lock())) {
                            fail = true;
                        }
                    }
                }
            }
        }

        for (auto const& i : map
                | filtered([](auto const& i) -> bool { return !i.second; })
        ) {
            auto const clazz
                = *get_as<scope::class_scope>(enclosing_scope_of(ctor));
            semantic_error(
                    ctor_def,
                    boost::format(
                        "  Failed to instantiate class template '%1%'\n"
                        "  Type of instance variable '%2%' can't be determined\n"
                        "  Note: Used contructor is at line:%3%, col:%4%"
                    ) % clazz->to_string() % i.first % ctor_def->line % ctor_def->col
                );
            fail = true;
        }

        auto const generate_default_initialize_ast
            = [&, this](auto const& name, auto const& type) -> ast::node::initialize_stmt
            {
                auto construct = generate_default_construct_ast(type, ctor_def->source_location());
                if (!construct) {
                    return nullptr;
                }

                auto decl = helper::make<ast::node::variable_decl>(false, name, boost::none);
                decl->set_source_location(*construct);
                {
                    auto const new_var = symbol::make<symbol::var_symbol>(decl, decl->name, !decl->is_var);
                    decl->symbol = new_var;
                    new_var->type = construct->type;
                    ctor->body->define_variable(new_var);
                }

                auto init = helper::make<ast::node::initialize_stmt>(
                            std::move(decl),
                            construct
                        );
                assert(init);
                init->set_source_location(*construct);

                return init;
            };

        // Note:
        // Check all non-initialized members are default constructible.
        for (auto const& v : map) {
            if (!is_initialized(v.first) && v.second) {
                if (!v.second.is_default_constructible()) {
                    semantic_error(
                            ctor_def,
                            boost::format(
                                "  Instance variable '@%1%' must be initialized explicitly in constructor because its type '%2%' is not default constructible"
                            ) % v.first % v.second.to_string()
                        );
                    fail = true;
                } else if (auto const t = type::get<type::class_type>(v.second)){
                    // Note:
                    // If the type is default constructible and not initialized yet,
                    // add default construction to the body of constructor
                    auto init = generate_default_initialize_ast('@' + v.first, *t);
                    if (!init) {
                        semantic_error(
                                ctor_def,
                                boost::format(
                                    "  Failed to generate default construction of type '%1%' for instance variable '@%2%'"
                                ) % v.second.to_string() % v.first
                            );
                        fail = true;
                    }

                    body_stmts.emplace(std::begin(body_stmts), std::move(init));
                }
            }
        }

        if (fail) {
            return boost::none;
        } else {
            return std::forward<Map>(map);
        }
    }

    template<class InstantiationMap>
    boost::optional<ast::node::class_definition>
    already_instantiated(ast::node::class_definition const& def, InstantiationMap const& map) const
    {
        auto const equals_to_def
            = [&](auto const& instantiated_def)
            {
                for (auto const& decl : instantiated_def->instance_vars) {
                    assert(!decl->symbol.expired());
                    auto const& t = decl->symbol.lock()->type;
                    auto const i = map.find(decl->name);
                    assert(i != std::end(map));
                    if (i->second != t) {
                        return false;
                    }
                }
                return true;
            };

        for (auto const& i : def->instantiated) {
            if (equals_to_def(i)) {
                return i;
            }
        }

        return boost::none;
    }

    template<class InstantiationMap>
    void substitute_class_template_params(
            ast::node::class_definition const& def,
            scope::class_scope const& scope,
            InstantiationMap const& map
        ) const
    {
        auto const find_from_map
            = [&map](auto const& predicate){ return helper::find_if(map, predicate); };

        for (auto const& s
                : scope->instance_var_symbols
                | filtered([](auto const& s){ return s->type.is_template(); })
            ) {
            // Note:
            // Replace instance variable's template type with instantiated type
            auto const var = find_from_map([&s](auto const& v){ return v.first == s->name; });
            assert(var);

            s->type = var->second;
        }

        for (auto const& ctor
                : def->member_funcs
                | filtered([](auto const& d){ return d->is_ctor(); })
            ) {
            for (auto const& p
                    : ctor->params
                    | filtered([](auto const& p){ return p->is_instance_var_init(); })) {
                auto const var = find_from_map([&p](auto const& v){ return p->name == ('@' + v.first); });
                assert(var && !var->second.is_template());

                p->type = var->second;
                assert(!p->param_symbol.expired());
                p->param_symbol.lock()->type = var->second;
            }
        }
    }

    template<class Types>
    helper::probable<scope::class_scope> instantiate_class_from_specified_param_types(
            scope::class_scope const& scope,
            Types const& specified
    ) {
        std::size_t const num_templates
            = boost::count_if(
                    scope->instance_var_symbols,
                    [](auto const& i){ return i->type.is_template(); }
                );

        if (specified.size() != num_templates) {
            return helper::oops_fmt(
                        "  Number of specified template types in object construction mismatches\n"
                        "  Note: You specified %1% but class '%2%' has %3% template(s)"
                    , specified.size(), scope->name, num_templates
                );
        }

        auto instantiation = generate_instantiation_map(scope);
        {
            auto i = std::begin(specified);
            for (auto const& s : scope->instance_var_symbols) {
                if (s->type.is_template()) {
                    instantiation[s->name] = *i;
                    ++i;
                }
            }
            assert(i == std::end(specified));
        }

        auto const def = scope->get_ast_node();
        auto const instantiated = prepare_class_definition_from_template(def, instantiation);
        assert(!instantiated->scope.expired());
        return instantiated->scope.lock();
    }

    template<class InstantiationMap>
    ast::node::class_definition prepare_class_definition_from_template(ast::node::class_definition const& def, InstantiationMap const& map)
    {
        if (auto const i = already_instantiated(def, map)) {
            return *i;
        }

        auto copied_def = ast::copy_ast(def);
        auto const enclosing_scope = enclosing_scope_of(def->scope.lock());
        failed += dispatch_forward_analyzer(copied_def, enclosing_scope, importer);
        assert(!copied_def->scope.expired());

        def->instantiated.push_back(copied_def);

        auto const copied_scope = copied_def->scope.lock();

        substitute_class_template_params(copied_def, copied_scope, map);

        walk_recursively_with(enclosing_scope_of(copied_scope), copied_def);

        return copied_def;
    }

    template<class CtorArgTypes, class InstantiationMap>
    std::pair<scope::class_scope, scope::func_scope/* ctor */>
    instantiate_class_template(
            ast::node::class_definition const& template_def,
             CtorArgTypes && arg_types,
             InstantiationMap const& template_instantiation
         )
     {
        assert(template_def->is_template());
        auto instantiated_def = prepare_class_definition_from_template(template_def, template_instantiation);
        assert(!instantiated_def->scope.expired());
        auto const instantiated_scope = instantiated_def->scope.lock();

        // Note:
        // Replace type of receiver with instantiated class
        arg_types[0] = instantiated_scope->type;

        // Note:
        // Re-resolve contructor for the instantiated class.
        auto const ctors_from_instantiated = instantiated_scope->resolve_ctor(arg_types);
        // TODO: check it's the only candidate.
        assert(!ctors_from_instantiated.empty());
        auto ctor_from_instantiated = *std::begin(ctors_from_instantiated);

        if (ctor_from_instantiated->is_template()) {
            std::tie(std::ignore, ctor_from_instantiated) = instantiate_function_from_template(ctor_from_instantiated->get_ast_node(), ctor_from_instantiated, arg_types);
            assert(!ctor_from_instantiated->is_template());
        }

        return {instantiated_scope, ctor_from_instantiated};
     }

    template<class InstanceVars, class CtorScope, class Body>
    bool walk_ctor_body_to_infer_class_template(InstanceVars const& vars, CtorScope const& ctor, Body &body)
    {
        // Note:
        // Obtain the information for template instance variable types from the constructor.
        // Types of instance variable which is initialized in parameter of ctor are already known.
        // At first replace the known types with its template types and visit body of ctor recursively,
        // then restore the template types not to affect the original AST of class template.

        std::vector<type::type> saved;
        for (auto const& v : vars) {
            saved.emplace_back(v->type);
        }

        for (auto const& p : ctor->params | filtered([](auto const& p){ return p->is_instance_var(); })) {
            auto const i
                = std::find_if(
                        std::begin(vars), std::end(vars),
                        [n = p->name.substr(1u)](auto const& v){ return n == v->name; }
                    );
            if (i == std::end(vars)) {
                continue;
            }

            (*i)->type = p->type;
        }

        BOOST_SCOPE_EXIT_ALL(&vars, &saved) {
            assert(vars.size() == saved.size());
            for (auto const& vt : helper::zipped(vars, saved)) {
                boost::get<0>(vt)->type = std::move(boost::get<1>(vt));
            }
        };

        return walk_recursively_with(ctor->body, body);
    }

    template<class ClassScope, class Types>
    auto visit_class_construct_impl(ClassScope &&scope, Types &&arg_types)
        -> helper::probable<std::pair<scope::class_scope, scope::func_scope>>
    {
        auto const ctor_candidates = scope->resolve_ctor(arg_types);

        if (ctor_candidates.empty()) {
            return helper::oops("  No matching constructor to construct class '" + scope->to_string() + "'");
        } else if (ctor_candidates.size() > 1u) {
            std::string errmsg = "  Contructor candidates for '" + scope->to_string() + "' are ambiguous";
            for (auto const& c : ctor_candidates) {
                errmsg += "\n  Candidate: " + c->to_string();
            }
            return helper::oops(errmsg);
        }

        auto ctor = *std::begin(std::move(ctor_candidates));
        auto ctor_def = ctor->get_ast_node();

        // Note:
        // If the constructor is not visited yet, visit it first to analyze initializations
        // in the body of constructor.
        if (!already_visited(ctor_def)) {
            if (!walk_recursively_with(enclosing_scope_of(ctor), ctor_def)) {
                return helper::oops_fmt("  Failed to analyze constructor '%1%' defined at line:%2%, col:%3%"
                        , ctor->to_string(), ctor_def->line, ctor_def->col
                );
            }
        }

        // Note:
        // Ignore if the parameter's type is class template or not because the class template is
        // never resolved at this point. It will be resolved after the class is instantiated.
        if (ctor->is_template()) {
            // Note:
            // The result of this instantiation may be function template.
            // See comment in instantiate_function_from_template().
            if (auto instantiated = already_instantiated_func(ctor_def, arg_types)) {
                ctor_def = *instantiated;
                ctor = ctor_def->scope.lock();
            } else {
                std::tie(ctor_def, ctor) = instantiate_function_from_template(ctor_def, ctor, arg_types);
            }
        }

        if (ctor->is_template() && scope->is_template() && !already_visited_ctor(ctor_def)) {
            // Note:
            // If the receiver is class template, the body of ctor is not visited yet
            // because the ctor is a template.  But initialize statements in the ctor
            // is necessary to determine the class instantiation.  So visit it here.
            if (!walk_ctor_body_to_infer_class_template(scope->instance_var_symbols, ctor, ctor_def->body)) {
                return helper::oops_fmt("  Failed to analyze constructor '%1%' defined at line:%2%, col:%3%"
                        , ctor->to_string(), ctor_def->line, ctor_def->col
                );
            }
            already_visited_ctors.insert(ctor_def);
        }

        auto const instantiation_success = check_template_instantiation_with_ctor(generate_instantiation_map(scope), ctor, ctor_def);
        if (!instantiation_success) {
            // Note:
            // Error message was already omitted in check_template_instantiation_with_ctor()
            return helper::oops_fmt(
                    "  Failed to instantiate class '%1%' with constructor '%2%'"
                    , scope->to_string(), ctor->to_string()
                );
        }
        auto const& template_instantiation = *instantiation_success;

        if (scope->is_template()) {
            std::tie(scope, ctor) = instantiate_class_template(scope->get_ast_node(), std::forward<Types>(arg_types), template_instantiation);
        }

        return std::make_pair(std::forward<ClassScope>(scope), ctor);
    }

    void visit_class_construct(ast::node::object_construct const& obj, type::class_type const& type)
    {
        assert(type->param_types.empty());

        std::vector<type::type> arg_types = {type};
        arg_types.reserve(obj->args.size()+1);

        for (auto const& a : obj->args) {
            auto const t = type_of(a);
            if (!t) {
                obj->type = type::type{};
                return;
            }

            arg_types.push_back(t);
        }

        auto const result = visit_class_construct_impl(type->ref.lock(), std::move(arg_types));

        if (auto const error = result.get_error()) {
            semantic_error(obj, *error);
            obj->type = type::type{};
            return;
        }

        auto const class_and_ctor = result.get_unsafe();

        obj->constructed_class_scope = class_and_ctor.first;
        obj->callee_ctor_scope = class_and_ctor.second;
        obj->type = class_and_ctor.first->type;
    }

    template<class Walker>
    void visit(ast::node::object_construct const& obj, Walker const& w)
    {
        obj->type = from_type_node(obj->obj_type);
        if (!obj->type) {
            semantic_error(obj, "  Invalid type for object construction");
            return;
        }

        w();

        if (!instantiate_all(obj->type, obj)) {
            return;
        }

        if (auto const maybe_class_type = type::get<type::class_type>(obj->type)) {
            visit_class_construct(obj, *maybe_class_type);
        } else if (auto const err = detail::ctor_checker{}(obj->type, obj->args)) {
            semantic_error(obj, *err);
        }
    }

    template<class Walker>
    void visit(ast::node::return_stmt const& ret, Walker const& w)
    {
        w();

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
    void visit(ast::node::for_stmt const& for_, Walker const& w)
    {
        w(for_->iter_vars, for_->range_expr);

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
                                boost::format(
                                    "  Type of '%1%' mismatches\n"
                                    "  Note: Type of '%1%' is '%2%' but the range requires '%3%'"
                                ) % param->name
                                  % param->type.to_string()
                                  % type::to_string(t)
                            );
                    }
                } else {
                    param->type = t;
                    assert(!param->param_symbol.expired());
                    param->param_symbol.lock()->type = t;
                }
            };

        auto const check_element =
            [this, &for_, &substitute_param_type](auto const& elem_type)
            {
                auto const iter_var_size = for_->iter_vars.size();

                if (auto const maybe_elem_tuple_type = type::get<type::tuple_type>(elem_type)) {
                    auto const& elem_tuple_type = *maybe_elem_tuple_type;
                    if (iter_var_size != 1 && elem_tuple_type->element_types.size() != iter_var_size) {
                        semantic_error(
                                for_,
                                boost::format(
                                    "  Number of variables and elements of range in 'for' statement mismatches\n"
                                    "  Note: %1% variables but %2% elements"
                                ) % for_->iter_vars.size()
                                  % elem_tuple_type->element_types.size()
                            );
                        return;
                    }

                    helper::each(substitute_param_type
                                , for_->iter_vars
                                , elem_tuple_type->element_types);

                } else {
                    if (iter_var_size != 1) {
                        semantic_error(for_, "  Number of variable here in 'for' statement must be 1");
                        return;
                    }
                    substitute_param_type(for_->iter_vars[0], elem_type);
                }
            };

        if (auto const maybe_array_range_type = type::get<type::array_type>(range_t)) {
            check_element((*maybe_array_range_type)->element_type);
        } else if (auto const maybe_class_type = type::get<type::class_type>(range_t)) {
            auto const& t = *maybe_class_type;

            // Note: Check size(rng : range_type)
            {
                auto const resolved = resolve_func_call(
                            "size",
                            std::vector<type::type>{t}
                        );

                if (auto const error = resolved.get_error()) {
                    semantic_error(for_, *error);
                    semantic_error(
                            for_,
                            "  Invalid range type '" + t->to_string() + "' to iterate in 'for' statement.  Range type must have size() member function"
                        );
                    return;
                }

                auto const callee = resolved.get_unsafe();
                for_->size_callee_scope = callee;

                assert(callee->ret_type);
                if (!callee->ret_type->is_builtin("uint")) {
                    semantic_error(
                            for_,
                            "  Invalid range type '" + t->to_string() + "' to iterate in 'for' statement.  '" + callee->to_string() + "' must return 'uint' value"
                        );
                    return;
                }
            }

            // Note: Check [](idx : uint, rng : range_type)
            {
                auto const resolved = resolve_func_call(
                            "[]",
                            std::vector<type::type>{
                                t,
                                type::get_builtin_type("uint", type::no_opt)
                            }
                        );

                if (auto const error = resolved.get_error()) {
                    semantic_error(for_, *error);
                    semantic_error(
                            for_,
                            "  Invalid range type '" + t->to_string() + "' to iterate in 'for' statement.  Range type must have [] operator"
                        );
                    return;
                }

                auto const callee = resolved.get_unsafe();
                for_->index_callee_scope = callee;

                assert(callee->ret_type);
                check_element(*callee->ret_type);
            }
        } else {
            semantic_error(for_, boost::format("  Range to iterate in for statement must be range, array or dictionary but actually '%1%'") % range_t.to_string());
        }

        w(for_->body_stmts);
    }

    void check_condition_expr(ast::node::any_expr const& expr)
    {
        apply_lambda(
            [this](auto const& e)
            {
                if (e->type != type::get_builtin_type("bool", type::no_opt)) {
                    semantic_error(e, boost::format("  Type of condition must be bool but actually '%1%'") % e->type.to_string());
                }
            }
            , expr
        );
    }

    template<class Walker>
    void visit(ast::node::while_stmt const& while_, Walker const& w)
    {
        w();
        check_condition_expr(while_->condition);
    }

    template<class Walker>
    void visit(ast::node::if_stmt const& if_, Walker const& w)
    {
        w();
        check_condition_expr(if_->condition);
        for (auto const& elseif : if_->elseif_stmts_list) {
            check_condition_expr(elseif.first);
        }
    }

    template<class Walker>
    void visit(ast::node::postfix_if_stmt const& postfix_if, Walker const& w)
    {
        w();
        check_condition_expr(postfix_if->condition);
    }

    template<class Walker>
    void visit(ast::node::case_stmt const& case_, Walker const& w)
    {
        w();
        for (auto const& when : case_->when_stmts_list) {
            check_condition_expr(when.first);
        }
    }

    template<class Walker>
    void visit(ast::node::switch_stmt const& switch_, Walker const& w)
    {
        w();

        auto const switcher_type = type_of(switch_->target_expr);
        for (auto const& when : switch_->when_stmts_list) {
            switch_->when_callee_scopes.emplace_back();
            auto &callees = switch_->when_callee_scopes.back();

            for (auto const& cond : when.first) {
                auto const t = type_of(cond);

                if (switcher_type.is_builtin() && t.is_builtin()) {
                    callees.emplace_back();
                    auto const ret_type = visit_builtin_binary_expr(ast::node::location_of(cond), "==", switcher_type, t);
                    assert(!ret_type || ret_type.is_builtin("bool"));
                    continue;
                }

                auto const resolved = resolve_func_call("==", std::vector<type::type>{switcher_type, t});
                if (auto const error = resolved.get_error()) {
                    auto const location = ast::node::location_of(cond);
                    semantic_error(location, *error);
                    semantic_error(location, "  In 'when' clause of case-when statement");
                    continue;
                }

                auto const callee = resolved.get_unsafe();

                if (!callee->ret_type || !callee->ret_type->is_builtin("bool")) {
                    auto const s = callee->ret_type ? callee->ret_type->to_string() : "UNKNOWN";
                    auto const def = callee->get_ast_node();
                    semantic_error(
                            ast::node::location_of(cond),
                            boost::format(
                                "  Compare operator '==' must returns 'bool' but actually returns '%1%' in 'when' clause\n"
                                "  Note: '%2%' is used, which is defined in line:%3%, col:%4%"
                                ) % s % callee->to_string() % def->line % def->col
                        );
                    continue;
                }

                callees.emplace_back(callee);
            }
        }

        assert(switch_->when_stmts_list.size() == switch_->when_callee_scopes.size());
    }

    template<class Walker>
    void visit(ast::node::initialize_stmt const& init, Walker const& w)
    {
        assert(init->var_decls.size() > 0);

        w();

        if (!init->maybe_rhs_exprs) {
            for (auto const& v : init->var_decls) {
                if (v->symbol.expired()) {
                    // When it fails to define the variable
                    return;
                }
                auto const& t = v->symbol.lock()->type;

                if (!t) {
                    semantic_error(v, boost::format("  Type of '%1%' can't be deduced") % v->name);
                    return;
                }

                if (!instantiate_all(t, v)) {
                    return;
                }

                if (!t.is_default_constructible()) {
                    semantic_error(
                            v,
                            boost::format("  Variable '%1%' must be initialized explicitly because type '%2%' is not default constructible")
                                % v->name % t.to_string()
                        );
                } else if (auto const clazz = type::get<type::class_type>(t)) {
                    auto default_construct = generate_default_construct_ast(*clazz, v->source_location());
                    if (default_construct) {
                        init->maybe_rhs_exprs = std::vector<ast::node::any_expr>{
                                default_construct
                            };
                    } else {
                        semantic_error(
                                v,
                                boost::format("  Generate default construction of type '%1%' to initialize '%2%' failed")
                                    % t.to_string() % v->name
                            );
                    }
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

        auto const maybe_ctor = enclosing_ctor();

        auto const substitute_type =
            [this, &init, &maybe_ctor](auto const& decl, type::type const& t)
            {
                if (decl->name == "_" && decl->symbol.expired()) {
                    return;
                }
                auto const symbol = decl->symbol.lock();

                if (decl->is_instance_var()) {
                    assert(maybe_ctor);
                    auto const& ctor = *maybe_ctor;
                    assert((ctor->params[0]->name == "self") && type::is_a<type::class_type>(ctor->params[0]->type));
                    auto const receiver_type = *type::get<type::class_type>(ctor->params[0]->type);
                    auto const receiver_scope = receiver_type->ref.lock();
                    auto const instance_var = receiver_scope->resolve_instance_var(decl->name.substr(1u));
                    assert(instance_var);
                    if (type::is_a<type::template_type>((*instance_var)->type)) {
                        // Note:
                        // Substitute the type which reveals from the rhs of initialize statement
                        // to class's template instance variable.  It reaches here wnly when analyzed
                        // by walk_ctor_body_to_infer_class_template().
                        //
                        // Note:
                        // This is dangerous operation because it changes the original class definition.
                        // But it's safe because the type of instance variables and self type are saved
                        // and will be restored after the analysis in walk_ctor_body_to_infer_class_template().
                        (*instance_var)->type = t;
                        // Note:
                        // If any param type are specified as template type, it should also be updated.
                        // So ensure any type not to be specified.
                        assert(receiver_type->param_types.empty());
                    }
                }

                if (!symbol->type || symbol->type == t) {
                    symbol->type = t;
                    return;
                }

                if (auto const c = type::get<type::class_type>(symbol->type)) {
                    if (t.is_instantiated_from(*c)) {
                        symbol->type = t;
                        return;
                    }
                } else if (auto const a = type::get<type::array_type>(symbol->type)) {
                    if (auto const rhs_a = type::get<type::array_type>(t)) {
                        if (type::is_instantiated_from(*rhs_a, *a)
                                || (*a)->element_type == (*rhs_a)->element_type) {
                            symbol->type = t;
                            return;
                        }
                    }
                } else if (auto const p1 = type::get<type::pointer_type>(symbol->type)) {
                    if (auto const p2 = type::get<type::pointer_type>(t)) {
                        if (type::is_instantiated_from(*p1, *p2)
                                || (*p1)->pointee_type == (*p2)->pointee_type) {
                            symbol->type = t;
                            return;
                        }
                    }
                }

                semantic_error(
                        init,
                        boost::format(
                            "  Types mismatch on substituting '%1%'\n"
                            "  Note: Type of '%1%' is '%2%' but rhs type is '%3%'"
                        ) % symbol->name
                            % symbol->type.to_string()
                            % type::to_string(t)
                    );
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
                    semantic_error(
                            init,
                            boost::format(
                                "  Rhs type of initialization here must be tuple\n"
                                "  Note: Rhs type is %1%"
                            ) % type_of(rhs_exprs[0]).to_string()
                        );
                    return;
                }

                auto const rhs_type = *maybe_rhs_type;
                if (rhs_type->element_types.size() != init->var_decls.size()) {
                    semantic_error(
                            init,
                            boost::format(
                                "  Size of elements in lhs and rhs mismatch\n"
                                "  Note: Size of lhs is %1%, size of rhs is %2%"
                            ) % init->var_decls.size()
                              % rhs_type->element_types.size()
                        );
                    return;
                }

                helper::each([&](auto const& elem_t, auto const& decl)
                                { substitute_type(decl, elem_t); }
                            , rhs_type->element_types
                            , init->var_decls);
            } else {
                if (init->var_decls.size() != rhs_exprs.size()) {
                    semantic_error(
                            init,
                            boost::format(
                                "  Size of elements in lhs and rhs mismatch\n"
                                "  Note: Size of lhs is %1%, size of rhs is %2%"
                            ) % init->var_decls.size()
                              % rhs_exprs.size()
                        );
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
    void visit(ast::node::assignment_stmt const& assign, Walker const& w)
    {
        assert(assign->assignees.size() == assign->rhs_exprs.size());
        assert(assign->op == "=");

        ast::walk_topdown(assign->assignees, var_ref_marker_for_lhs_of_assign{});

        w();

        // Note:
        // Do not check assignees' types because of '_' variable

        for (auto const& e : assign->rhs_exprs) {
            if (!type_of(e)) {
                if (assign->rhs_tuple_expansion) {
                    semantic_error(
                            assign,
                            "  Error on assignment to multiple values with tuple expansion"
                        );
                }
                return;
            }
        }

        // Check assignees' immutablity
        // TODO:
        // Use walker
        for (auto const& lhs : assign->assignees) {

            auto const the_var_ref = var_ref_getter_for_lhs_of_assign{}.visit(lhs);
            if (!the_var_ref) {
                semantic_error(assign, "  Lhs of assignment must be variable access, index access or member access.");
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
                semantic_error(assign, boost::format("  Can't assign to immutable variable '%1%'") % var_sym->name);
                return;
            }
        }

        helper::each(
                [&](auto const& lhs, auto const& rhs)
                {
                    auto const rhs_type = type_of(rhs);

                    if (auto const a = get_as<ast::node::index_access>(lhs)) {
                        // Note:
                        // Edge case for lhs of assignment
                        //    {expr}[{expr}] = rhs

                        auto const& access = *a;
                        auto const idx_type = type_of(access->index_expr);

                        if (!idx_type) {
                            return;
                        }

                        index_access_analyzer<symbol_analyzer> analyzer {
                                access,
                                idx_type,
                                *this
                            };

                        if (auto const error = analyzer.analyze_lhs_of_assign(rhs_type)) {
                            semantic_error(access, *error);
                            semantic_error(access, "  Error on assignment to indexed value");
                        }

                        return;
                    }

                    auto const lhs_type = type_of(lhs);

                    if (!lhs_type || !rhs_type) {
                        // Note:
                        // When lhs is '_' variable
                        // When rhs has an error

                        return;
                    }

                    if (lhs_type != rhs_type) {
                        semantic_error(
                                assign,
                                boost::format(
                                    "  Types mismatch on assignment '%1%'\n"
                                    "  Note: Type of lhs is '%2%', type of rhs is '%3%'"
                                ) % assign->op
                                % type::to_string(lhs_type)
                                % type::to_string(rhs_type)
                            );
                        if (assign->rhs_tuple_expansion) {
                            semantic_error(
                                    assign,
                                    "  Error on assignment to multiple values with tuple expansion"
                                );
                        }
                    }
                },
                assign->assignees,
                assign->rhs_exprs
            );
    }

    template<class Root>
    auto resolve_lambda(Root const& root)
    {
        detail::lambda_resolver resolver;

        for (auto &l : lambdas) {
            ast::walk_topdown(l, resolver);
        }

        for (auto const& l : lambdas) {
            root->functions.push_back(std::move(l->def));
        }

        return std::move(resolver).get_captures();
    }

    auto get_main_arg_ctor()
    {
        return std::move(main_arg_ctor);
    }

    template<class Walker>
    void visit(ast::node::class_definition const& class_def, Walker const& w)
    {
        if (already_visited(class_def)) {
            return;
        }
        already_visited_classes.insert(class_def);

        introduce_scope_and_walk(class_def->scope.lock(), w, class_def->instance_vars);

        auto const scope = class_def->scope.lock();

        for (auto const& s : scope->instance_var_symbols) {
            if (!s->type) {
                return;
            }
        }

        if (class_def->is_template()) {
            // Class templates are not actually instantiated
            return;
        }

        assert(!class_def->scope.expired());
        introduce_scope_and_walk(scope, w, class_def->member_funcs);
    }

    template<class Walker>
    void visit(ast::node::parameter const& param, Walker const& w)
    {
        if (param->param_type) {
            // Note:
            // 'param' has a type which is generated from type::from_ast()
            if (!instantiate_all(param->type, param)) {
                return;
            }
        }

        w();
    }

    template<class T, class Walker>
    void visit(T const&, Walker const& walker)
    {
        // simply visit children recursively
        walker();
    }
};


} // namespace detail

semantics_context check_semantics(ast::ast &a, scope::scope_tree &t, syntax::importer &i)
{
    detail::symbol_analyzer resolver{t.root, t.root, i};
    ast::walk_topdown(a.root, resolver);
    resolver.analyze_main_func();
    auto const failed = resolver.num_errors();

    if (failed > 0) {
        throw semantic_check_error{failed, "symbol resolution"};
    }

    // Note:
    // Aggregate initialization here makes clang 3.4.2 crash.
    // I avoid it by explicitly specifying 'semantics_context'.
    return semantics_context{t, resolver.resolve_lambda(a.root), resolver.get_main_arg_ctor()};
}

} // namespace semantics
} // namespace dachs
