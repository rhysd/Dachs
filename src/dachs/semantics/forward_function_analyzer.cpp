#include <cstddef>
#include <cassert>
#include <vector>

#include "dachs/ast/ast.hpp"
#include "dachs/semantics/forward_function_analyzer.hpp"

namespace dachs {
namespace semantics {
namespace detail {

class forward_function_analyzer {

    std::vector<ast::node::global_definition> &definitions;
    std::size_t failed = 0u;

    template<class Node>
    std::string lambda_name_for(Node const& n) const noexcept
    {
        return "";
    }

public:

    forward_function_analyzer(decltype(definitions) &defs, Walker &w) noexcept
        : definitions(defs), walker(w)
    {}

    std::size_t num_failed() const noexcept
    {
        return failed;
    }

    template<class Walker>
    void visit(ast::node::func_invocation const& invocation, Walker const& w)
    {
        if (invocation->do_block) {
            auto &block = *invocation->do_block;
            assert(block->name == "");
            block->name = lambda_name_for(block);
            definitions.push_back(block);

            // Note:
            // Walk recursively
            ast::make_walker(*this).walk(*invocation->do_block);
        }

        w();
    }

    template<class Node, class Walker>
    void visit(T const&, Walker const& w)
    {
        w();
    }
};

} // namespace detail

std::size_t dispatch_forward_function_analyzer(ast::ast const& a)
{
    return 0u;
}

} // namespace semantics
} // namespace dachs
