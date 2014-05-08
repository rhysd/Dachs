#include <cstddef>
#include <iostream>

#include "dachs/ast_walker.hpp"
#include "dachs/semantics_check.hpp"
#include "dachs/exception.hpp"

namespace dachs {
namespace semantics {

namespace detail {

using std::size_t;

class semantics_checker {

public:

    std::size_t failed = 0;

    template<class Walker>
    void visit(ast::node::function_definition const& func_def, Walker const& recursive_walker) noexcept
    {
        if (func_def->kind == ast::symbol::func_kind::proc && func_def->return_type) {
            std::cerr << "Semantic error at line:" << func_def->line << ", col:" << func_def->col << "\nproc '" << func_def->name << "' can't have return type" << std::endl;
            failed++;

            // TODO: Check that procedures don't have any return statement
        }

        recursive_walker();
    }

    template<class T, class Walker>
    void visit(T const&, Walker const& w) noexcept
    {
        w();
    }
};

} // namespace detail

bool check_semantics(ast::ast &a)
{
    detail::semantics_checker checker;
    ast::walk_topdown(a.root, checker);

    if (checker.failed > 0) {
        throw dachs::semantic_check_error{checker.failed, "semantics check"};
    }

    return checker.failed == 0;
}

} // namespace semantics
} // namespace dachs
