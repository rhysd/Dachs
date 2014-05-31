#include <cassert>
#include <cstddef>
#include <iterator>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/algorithm/transform.hpp>
#include <boost/range/numeric.hpp>
#include <boost/algorithm/string/join.hpp>

#include "dachs/semantics/scope.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"

namespace dachs {
namespace scope_node {
namespace detail {

template<class FuncScope, class ArgTypes>
inline
std::size_t get_overloaded_function_score(FuncScope const& func, ArgTypes const& arg_types)
{
    if (arg_types.size() != func->params.size()) {
        return 0u;
    }

    std::size_t score = 1u;
    auto const func_def = func->get_ast_node();

    if (arg_types.size() == 0) {
        return score;
    }

    // Calculate the score of arguments' coincidence
    std::vector<std::size_t> v;
    v.reserve(arg_types.size());

    boost::transform(arg_types, func_def->params, std::back_inserter(v),
            [](auto const& arg_type, auto const& param)
            {
                assert(arg_type);
                if (type::has<type::template_type>(param->type)) {
                    // Function parameter is template.  It matches any types.
                    return 1u;
                }

                assert(param->type);

                if (param->type == arg_type) {
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
            , std::vector<type::type> const& arg_types) const
{
    boost::optional<scope::func_scope> result = boost::none;

    std::size_t score = 0u;
    for (auto const& f : functions) {
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

ast::node::function_definition func_scope::get_ast_node() const noexcept
{
    auto maybe_func_def = ast::node::get_shared_as<ast::node::function_definition>(ast_node);
    assert(maybe_func_def);
    return *maybe_func_def;
}

// Note:
// define_function() can't share the implementation with resolve_func()'s overload resolution because define_function() considers new function's template arguments
bool func_scope::operator==(func_scope const& rhs) const noexcept
{
    if (name != rhs.name || params.size() != rhs.params.size()) {
        return false;
    }

    auto const lhs_func_def = get_ast_node();
    auto const rhs_func_def = rhs.get_ast_node();

    // Check arguments' types
    {
        // Note:
        // Do not use std::cbegin()/std::cend() because of libstdc++
        auto ritr = lhs_func_def->params.cbegin();
        auto litr = rhs_func_def->params.cbegin();
        auto const rend = lhs_func_def->params.cend();
        auto const lend = rhs_func_def->params.cend();
        for (; ritr != rend && litr != lend; ++ritr, ++litr) {
            auto const& left_type = (*litr)->type;
            auto const& right_type = (*ritr)->type;

            if (left_type.is_template() && right_type.is_template()) {
                // Both sides are not template
                if (left_type != right_type) {
                    return false;
                }
            } else if (!left_type.is_template() && !right_type.is_template()) {
                // Both side are template
                continue;
            } else {
                // One side is template and other side is not a template
                return false;
            }
        }
    }

    // Note:
    // Reach here when arguments match completely

    return true;
}

std::string func_scope::to_string() const noexcept
{
    auto const def = get_ast_node();
    std::string ret = ast::symbol::to_string(def->kind) + ' ' + name + '(';

    ret += boost::algorithm::join(def->params | boost::adaptors::transformed([](auto const& p){ return p->type.to_string(); }), ", ");
    ret += ')';

    if (def->ret_type) {
        ret += ": " + def->ret_type->to_string();
    }

    return ret;
}

} // namespace scope_node

} // namespace dachs
