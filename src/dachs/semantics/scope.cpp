#include <cassert>
#include <cstddef>
#include <iterator>
#include <unordered_map>

#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/numeric.hpp>
#include <boost/algorithm/string/join.hpp>

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

namespace detail {

template<class FuncScope, class ArgTypes>
inline
std::size_t get_overloaded_function_score(FuncScope const& func, ArgTypes const& arg_types)
{
    if (arg_types.size() != func->params.size()) {
        return 0u;
    }

    std::size_t score = 1u;

    if (arg_types.size() == 0) {
        return score;
    }

    // Calculate the score of arguments' coincidence
    std::vector<std::size_t> v;
    v.reserve(arg_types.size());

    boost::transform(arg_types, func->params, std::back_inserter(v),
            [](auto const& arg_type, auto const& param)
            {
                assert(arg_type);
                if (type::is_a<type::template_type>(param->type)) {
                    // Note:
                    // Function parameter is template.  It matches any types.
                    return 1u;
                }

                assert(param->type);

                if (param->type.is_class_template() && type::is_a<type::class_type>(arg_type)) {
                    auto const lhs_class = *type::get<type::class_type>(param->type);
                    auto const rhs_class = *type::get<type::class_type>(arg_type);
                    if (lhs_class->name == rhs_class->name) {
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

                        return 2u;
                    }
                }

                if (param->type == arg_type) {
                    return 3u;
                } else {
                    return 0u;
                }
            });

    // Note:
    // If the function have no argument and return type is not specified, score is 1.
    return boost::accumulate(v, score, [](auto l, auto r){ return l*r; });
}

template<class Funcs, class Types>
auto get_overloaded_function(Funcs const& candidates, std::string const& name, Types const& arg_types)
{
    boost::optional<scope::func_scope> result = boost::none;

    std::size_t score = 0u;
    for (auto const& f : candidates) {
        if (f->name != name) {
            continue;
        }

        auto const score_tmp = detail::get_overloaded_function_score(f, arg_types);
        if (score_tmp > score) {
            score = score_tmp;
            result = f;
        }
    }

    return result;
}

} // namespace detail

global_scope::maybe_func_t global_scope::resolve_func(std::string const& name, std::vector<type::type> const& arg_types) const
{
    return detail::get_overloaded_function(functions, name, arg_types);
}

global_scope::maybe_class_t global_scope::resolve_class_template(std::string const& name, std::vector<type::type> const& specified) const
{
    auto const c = resolve_class(name);
    if (!c || !(*c)->is_template()) {
        return boost::none;
    }

    // Note:
    // Remember all template parameters' index and its specified types
    std::unordered_map<std::size_t, type::type> specified_template_params;
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
