#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>
#include <iostream>

#include "dachs/scope.hpp"
#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"
#include "dachs/ast_walker.hpp"

namespace dachs {
namespace scope {

namespace detail {

// AST walker to generate a scope tree
struct scope_tree_generator {

    enclosing_scope_type current_scope;

    template<class T>
    void visit(T const&)
    {
        // Do nothing as default behavior
    }
};

} // namespace detail

scope_tree make_scope_tree(ast::ast &a)
{
    global_scope tree_root;
    detail::scope_tree_generator generator{tree_root};
    ast::walker<detail::scope_tree_generator> walker{generator};
    walker.walk(a.root);
    return scope_tree{tree_root};
}

std::string dump_scope_tree(scope_tree const&)
{
    return ""; // TODO
}

} // namespace scope
} // namespace dachs
