#if !defined DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
#define      DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED

#include <cstddef>
#include <unordered_map>
#include <iostream>
#include <utility>
#include <string>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/scope.hpp"

namespace dachs {
namespace semantics {

namespace tags {

struct invocation{};
struct offset{};

} // namespace tags

namespace mi = boost::multi_index;

using offset_map_elem_type = std::pair<ast::node::ufcs_invocation, std::size_t>;
using captured_offset_map
    = boost::multi_index_container<
            offset_map_elem_type,
            mi::indexed_by<
                mi::ordered_unique<
                        mi::tag<tags::invocation>,
                        mi::member<offset_map_elem_type, ast::node::ufcs_invocation, &offset_map_elem_type::first>
                >,
                mi::ordered_unique<
                        mi::tag<tags::offset>,
                        mi::member<offset_map_elem_type, std::size_t, &offset_map_elem_type::second>
                >
            >
        >;
using lambda_captures_type = std::unordered_map<scope::func_scope, captured_offset_map>;

struct semantics_context {
    scope::scope_tree scopes;
    lambda_captures_type lambda_captures;

    semantics_context(semantics_context const&) = delete;
    semantics_context &operator=(semantics_context const&) = delete;
    semantics_context(semantics_context &&) = default;
    semantics_context &operator=(semantics_context &&) = default;

    void dump_lambda_captures() const noexcept
    {
        std::cout << "Lambda captures:" << std::endl;
        for (auto const& cs : lambda_captures) {
            std::cout << "  " << cs.first->to_string() << std::endl;
            for (auto const& c : cs.second.get<semantics::tags::offset>()) {
                std::cout << "    " << c.first->to_string() << ": " << c.second << std::endl;
            }
        }
    }
};

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
