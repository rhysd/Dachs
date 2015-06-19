#include <cassert>
#include <cstddef>
#include <iterator>
#include <tuple>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/algorithm/max_element.hpp>
#include <boost/range/numeric.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/semantics/scope.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/util.hpp"

namespace dachs {
namespace scope_node {

using boost::adaptors::transformed;
using boost::adaptors::filtered;
using boost::algorithm::all_of;
using std::size_t;


void basic_scope::warn_or_check_shadowing_var_recursively(maybe_var_t const& maybe_shadowing, symbol::var_symbol const& new_var) const
{
    if (maybe_shadowing) {
        auto const the_node = new_var->ast_node.get_shared();
        auto const prev_node = (*maybe_shadowing)->ast_node.get_shared();
        assert(the_node);
        if (prev_node) {
            output_warning(the_node, boost::format(
                            "  Shadowing variable '%1%'. It shadows a variable at %2%"
                        ) % new_var->name % prev_node->location.to_string()
                    );
        } else {
            output_warning(the_node, boost::format(
                            "  Shadowing variable '%1%'. It shadows a built-in variable"
                        ) % new_var->name
                    );
        }
    } else {
        apply_lambda(
            [&new_var](auto const& s){ s.lock()->check_shadowing_variable(new_var); },
            enclosing_scope
        );
    }
}

void basic_scope::check_shadowing_variable(symbol::var_symbol const& new_var) const
{
    apply_lambda(
        [&new_var](auto const& s)
        {
            s.lock()->check_shadowing_variable(new_var);
        }, enclosing_scope
    );
}

namespace detail {

struct template_depth_calculator : boost::static_visitor<unsigned int> {
    result_type apply_recursively(type::type const& t) const
    {
        return t.apply_visitor(*this);
    }

    result_type operator()(type::class_type const& clazz) const
    {
        result_type depth = 1u;
        for (auto const& t : clazz->param_types) {
            auto const d = apply_recursively(t) + 1u;
            if (depth < d) {
                depth = d;
            }
        }
        return depth;
    }

    result_type operator()(type::array_type const& array) const
    {
        return 1u + apply_recursively(array->element_type);
    }

    result_type operator()(type::pointer_type const& ptr) const
    {
        return 1u + apply_recursively(ptr->pointee_type);
    }

    result_type operator()(type::tuple_type const& tuple) const
    {
        result_type depth = 1u;
        for (auto const& t : tuple->element_types) {
            auto const d = apply_recursively(t) + 1u;
            if (depth < d) {
                depth = d;
            }
        }
        return depth;
    }

    result_type operator()(type::template_type const&)
    {
        return 0u;
    }

    template<class T>
    result_type operator()(T const&) const
    {
        return 1u;
    }
};

auto get_parameter_score(type::type const& arg_type, type::type const& param_type)
{
    assert(arg_type);
    assert(param_type);

    if (type::is_a<type::template_type>(param_type)) {
        // Note:
        // Function parameter is template.  It matches any types.
        return std::make_tuple(1u, 0u, 0u);
    }

    if (arg_type.is_instantiated_from(param_type)) {
        // Note:
        // When the lhs parameter is class template and the rhs argument is
        // class which is instantiated from the same class template, they match
        // more strongly than simple template match and more weakly than the
        // perfect match.
        //   e.g.
        //      class Foo
        //          a
        //      end
        //
        //      func foo(a : Foo)
        //      end
        //
        //      func main
        //          foo(new Foo{42})  # Calls foo(Foo(int))
        //      end

        // Note:
        // This matching is used in a receiver of member function
        //
        //   e.g.
        //      class Foo
        //          a
        //
        //          func foo
        //          end
        //      end
        //
        //  The member function foo() is defined actually like below
        //
        //      func foo(self : Foo)
        //      end
        //
        //  Actually '(new Foo{42}).foo()' means calling foo(Foo(int)) by UFCS
        template_depth_calculator calculator;
        return std::make_tuple(2u, 0u, param_type.apply_visitor(calculator));
    }

    if (param_type == arg_type) {
        return std::make_tuple(3u, 1u, 0u);
    } else {
        return std::make_tuple(0u, 0u, 0u);
    }
}

template<class FuncScope, class ArgTypes>
inline
std::tuple<size_t, size_t, size_t>
get_overloaded_function_score(FuncScope const& func, ArgTypes const& arg_types)
{
    size_t score = 1u;
    size_t num_perfect_match = 0u;
    size_t total_template_depth = 0u;

    if (arg_types.size() == 0) {
        return std::make_tuple(score, num_perfect_match, total_template_depth);
    }

    // Calculate the score of arguments' coincidence
    for (auto const& arg_type_and_param : helper::zipped(arg_types, func->params)) {
        auto const param_score
            = get_parameter_score(
                    boost::get<0>(arg_type_and_param),
                    boost::get<1>(arg_type_and_param)->type
                );
        score *= std::get<0>(param_score);
        num_perfect_match += std::get<1>(param_score);
        total_template_depth += std::get<2>(param_score);
    }

    // Note:
    // If the function have no argument and return type is not specified, score is 1.
    return std::make_tuple(score, num_perfect_match, total_template_depth);
}

template<class Funcs, class Types>
auto generate_first_overload_set(Funcs const& candidates, std::string const& name, Types const& arg_types)
{
    std::vector<std::tuple<
        size_t, // matching score score
        size_t, // the number of perfect matching parameters
        size_t, // total template depth
        typename Funcs::value_type
    >> candidate_scores;
    size_t max_score = 0u;

    for (auto const& f : candidates) {
        if (f->name != name || arg_types.size() != f->params.size()) {
            continue;
        }

        auto const score_tmp = detail::get_overloaded_function_score(f, arg_types);
        if (std::get<0>(score_tmp) == 0u) {
            continue;
        }

        if (std::get<0>(score_tmp) >= max_score) {
            candidate_scores.emplace_back(std::get<0>(score_tmp), std::get<1>(score_tmp), std::get<2>(score_tmp), f);
            max_score = std::get<0>(score_tmp);
        }
    }

    // Note:
    // Narrow down candidates by the matching score
    helper::remove_erase_if(
            candidate_scores,
            [&](auto const& c){ return std::get<0>(c) < max_score; }
        );

    return candidate_scores;
}

template<size_t I, class Scores>
void narrow_down_score_by(Scores &scores) {
    auto const max
        = std::get<I>(
            *boost::max_element(
                scores,
                [](auto const& l, auto const& r){ return std::get<I>(l) < std::get<I>(r); }
            )
        );

    helper::remove_erase_if(
            scores,
            [&](auto const& c){ return std::get<I>(c) < max; }
        );
}

template<class Funcs, class Types>
auto generate_overload_set(Funcs const& candidates, std::string const& name, Types const& arg_types)
{
    auto candidate_scores = generate_first_overload_set(candidates, name, arg_types);

    // Note:
    // Narrow down candidates by the number of perfect match of argument types
    if (candidate_scores.size() > 1) {
        narrow_down_score_by<1>(candidate_scores);
    }

    // Note:
    // Narrow down candidates by the total depth of templates of argument types
    if (candidate_scores.size() > 1) {
        narrow_down_score_by<2>(candidate_scores);
    }

    std::unordered_set<typename Funcs::value_type> result;
    for (auto const& c : candidate_scores) {
        result.emplace(std::get<3>(c));
    }
    return result;
}

template<class Funcs, class Types>
inline function_set get_overloaded_function(Funcs const& candidates, std::string const& name, Types const& arg_types)
{
    return generate_overload_set(candidates, name, arg_types);
}

} // namespace detail

void global_scope::define_function(scope::func_scope const& new_func) noexcept
{
    // Do check nothing
    if (new_func->is_converter()) {
        cast_funcs.push_back(new_func);
    } else {
        functions.push_back(new_func);
    }
}

function_set global_scope::resolve_func(std::string const& name, std::vector<type::type> const& arg_types) const
{
    return detail::get_overloaded_function(functions, name, arg_types);
}

global_scope::maybe_func_t global_scope::resolve_cast_func(type::type const& from, type::type const& to) const
{
    global_scope::maybe_func_t result = boost::none;

    for (auto const& c : cast_funcs) {
        if (to != *c->ret_type) {
            continue;
        }
        auto const& t = c->params[0]->type;

        if (from == t) {
            return c;
        } else if (from.is_instantiated_from(t)) {
            result = c;
        }
    }

    return result;
}

global_scope::maybe_class_t global_scope::resolve_class_template(std::string const& name, std::vector<type::type> const& specified) const
{
    auto const c = resolve_class_by_name(name);
    if (!c || !(*c)->is_template()) {
        return boost::none;
    }

    // Note:
    // Remember all template parameters' index and its specified types
    std::unordered_map<size_t, type::type> specified_template_params;
    {
        auto itr = std::begin(specified);
        for (auto const idx : helper::indices((*c)->instance_var_symbols.size())) {
            auto const& s = (*c)->instance_var_symbols[idx];
            if (s->type.is_template()) {
                if (itr == std::end(specified)) {
                    return boost::none;
                }
                specified_template_params[idx] = *itr;
                ++itr;
            }
        }
        if (itr != std::end(specified)) {
            return boost::none;
        }
    }

    auto const def = (*c)->get_ast_node();
    for (auto const& instantiated_def : def->instantiated) {
        auto const instantiated = instantiated_def->scope.lock();
        if (all_of(
                specified_template_params,
                [&vars=instantiated->instance_var_symbols](auto const& param)
                {
                    return vars[param.first]->type == param.second;
                }
            )
        ) {
            return instantiated;
        }
    }

    return boost::none;
}


ast::node::function_definition func_scope::get_ast_node() const noexcept
{
    auto const maybe_func_def = ast::node::get_shared_as<ast::node::function_definition>(ast_node);
    assert(maybe_func_def);
    return *maybe_func_def;
}

boost::optional<scope::class_scope> func_scope::get_receiver_class_scope() const
{
    if (params.empty()) {
        return boost::none;
    }

    auto const t = type::get<type::class_type>(params[0]->type);
    if (!t) {
        return boost::none;
    }

    if ((*t)->ref.expired()) {
        return boost::none;
    }

    return (*t)->ref.lock();
}

// Note:
// define_function() can't share the implementation with resolve_func()'s overload resolution because define_function() considers new function's template arguments
bool func_scope::operator==(func_scope const& rhs) const noexcept
{
    if (name != rhs.name || params.size() != rhs.params.size()) {
        return false;
    }

    // Check arguments' types
    for (auto const& t : helper::zipped(params, rhs.params)) {
        auto const& left_type = boost::get<0>(t)->type;
        auto const& right_type = boost::get<1>(t)->type;

        // Note: Do not consider class template
        auto const lhs_is_template = type::is_a<type::template_type>(left_type);
        auto const rhs_is_template = type::is_a<type::template_type>(right_type);

        if (lhs_is_template && rhs_is_template) {
            continue;
        } else if (!lhs_is_template && !rhs_is_template) {
            if (left_type != right_type) {
                return false;
            }
        } else {
            // Note: One side is template and other side is not a template
            return false;
        }
    }

    // Note:
    // Reach here when arguments match completely

    return true;
}

std::string func_scope::to_string() const noexcept
{
    if (is_builtin) {
        return "func " + name + '('
            + boost::algorithm::join(
                    params | transformed(
                        [](auto const& p)
                        {
                            return p->name;
                        }
                    ), ", "
                )
            + ')';
    }

    auto const def = get_ast_node();
    std::string ret = ast::symbol::to_string(def->kind) + ' ' + name + '(';

    ret += boost::algorithm::join(def->params | transformed([](auto const& p){ return p->type.to_string(); }), ", ");
    ret += ')';

    if (def->ret_type) {
        ret += ": " + def->ret_type->to_string();
    }

    if (is_const()) {
        ret += " -> const";
    }

    return ret;
}

ast::node::class_definition class_scope::get_ast_node() const noexcept
{
    auto const maybe_func_def = ast::node::get_shared_as<ast::node::class_definition>(ast_node);
    assert(maybe_func_def);
    return *maybe_func_def;
}

std::string class_scope::to_string() const noexcept
{
    return "<class:" + name + ':' + helper::hex_string_of_ptr(this) + '>';
}

bool class_scope::operator==(class_scope const& rhs) const noexcept
{
    if ((name != rhs.name) || (instance_var_symbols.size() != rhs.instance_var_symbols.size())) {
        return false;
    }

    {
        auto i = std::begin(rhs.instance_var_symbols);
        for (auto const& s : instance_var_symbols) {
            if (s->type != (*i)->type) {
                return false;
            }
            ++i;
        }
    }

    return true;
}

} // namespace scope_node

} // namespace dachs
