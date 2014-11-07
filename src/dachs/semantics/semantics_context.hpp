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
#include "dachs/semantics/symbol.hpp"
#include "dachs/ast/ast_fwd.hpp"

namespace dachs {
namespace semantics {

namespace tags {

struct offset{};
struct introduced{};
struct refered_symbol{};

} // namespace tags

struct lambda_capture {
    ast::node::ufcs_invocation introduced;
    std::size_t offset;
    symbol::var_symbol refered_symbol;
};

namespace mi = boost::multi_index;

using captured_offset_map
    = boost::multi_index_container<
            lambda_capture,
            mi::indexed_by<
                mi::ordered_unique<
                        mi::tag<tags::offset>,
                        mi::member<lambda_capture, std::size_t, &lambda_capture::offset>
                >,
                mi::ordered_unique<
                        mi::tag<tags::introduced>,
                        mi::member<lambda_capture, ast::node::ufcs_invocation, &lambda_capture::introduced>
                >,
                mi::ordered_unique<
                        mi::tag<tags::refered_symbol>,
                        mi::member<lambda_capture, symbol::var_symbol, &lambda_capture::refered_symbol>
                >
            >
        >;
using lambda_captures_type = std::unordered_map<type::generic_func_type, captured_offset_map>;

struct semantics_context {
    scope::scope_tree scopes;
    lambda_captures_type lambda_captures;
    std::unordered_map<type::generic_func_type, ast::node::tuple_literal> lambda_instantiation_map;

    semantics_context(semantics_context const&) = delete;
    semantics_context &operator=(semantics_context const&) = delete;
    semantics_context(semantics_context &&) = default;
    semantics_context &operator=(semantics_context &&) = default;

    template<class Stream = std::ostream>
    void dump_lambda_captures(Stream &out = std::cerr) const noexcept
    {
        out << "Lambda captures:" << std::endl;
        for (auto const& cs : lambda_captures) {
            out << "  " << cs.first->to_string() << std::endl;
            for (auto const& c : cs.second.get<semantics::tags::offset>()) {
                out << "    " << c.refered_symbol->name << ':' << c.introduced->line << ':' << c.introduced->col << " -> " << c.introduced->member_name << std::endl;
            }
        }
    }
};

} // namespace semantics
} // namespace dachs

#endif    // DACHS_SEMANTICS_SEMANTICS_CONTEXT_HPP_INCLUDED
