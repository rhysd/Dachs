#include <cassert>
#include <cstddef>
#include <iterator>

#include <boost/range/algorithm/transform.hpp>
#include <boost/range/numeric.hpp>

#include "dachs/semantics/scope.hpp"
#include "dachs/ast/ast.hpp"
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
    auto const func_def = func->get_ast_node();

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

// Note:
// define_function() can't share the implementation with resolve_func()'s overload resolution because define_function() considers new function's template arguments
bool global_scope::define_function(scope::func_scope const& new_func, ast::node::function_definition const& new_func_def) noexcept
{
    // Check duplication considering overloaded functions
    for (auto const& f : functions) {

        if (new_func->params.size() == f->params.size()) {
            auto const func_def = f->get_ast_node();

            if (new_func_def->name == func_def->name) {
                // Check arguments' types
                {
                    // Note:
                    // Do not use std::cbegin()/std::cend() because of libstdc++
                    auto iter1 = func_def->params.cbegin();
                    auto iter2 = new_func_def->params.cbegin();
                    auto const end1 = func_def->params.cend();
                    auto const end2 = new_func_def->params.cend();
                    bool argument_mismatch = false;
                    for (; iter1 != end1 && iter2 != end2; ++iter1, ++iter2) {
                        auto const& maybe_type1 = (*iter1)->type;
                        auto const& maybe_type2 = (*iter2)->type;

                        if (maybe_type1 && maybe_type2) {
                            // Both sides are not template
                            assert(*maybe_type1);
                            assert(*maybe_type2);
                            if (*maybe_type1 != *maybe_type2) {
                                argument_mismatch = true;
                                break;
                            }
                        } else if (!maybe_type1 || !maybe_type2) {
                            // One side is template and other side is not a template
                            argument_mismatch = true;
                            break;
                        } else {
                            // Both side are template
                        }
                    }
                    if (argument_mismatch) {
                        // Go to next fuction candidate
                        continue; // Continues "for (auto const& f : functions) {" loop
                    }
                }

                // Note:
                // Reach here when arguments match completely

                if (!func_def->ret_type && !new_func_def->ret_type) {
                    print_duplication_error(func_def, new_func_def, func_def->name);
                    return false;
                } else if (func_def->ret_type && new_func_def->ret_type) {
                    assert(*func_def->ret_type);
                    assert(*new_func_def->ret_type);
                    if (*func_def->ret_type == *new_func_def->ret_type) {
                        print_duplication_error(func_def, new_func_def, func_def->name);
                        return false;
                    }
                } else {
                    // Return type doesn't match.  Go to next function candidate.
                }

            }
        }
    }

    functions.push_back(new_func);
    return true;
}

} // namespace scope_node

} // namespace dachs
