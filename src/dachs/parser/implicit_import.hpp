#if !defined DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED
#define      DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED

#include "dachs/ast/ast.hpp"

namespace dachs {
namespace syntax {

struct implicit_import {
    bool range_expr_found = false;
    bool array_found = false;

    void install(ast::node::inu const& program) const
    {
        if (range_expr_found) {
            program->imports.push_back(
                    helper::make<ast::node::import>("std.range")
                );
        }

        if (array_found) {
            program->imports.push_back(
                    helper::make<ast::node::import>("std.array")
                );
        }
    }
};

} // namespace syntax
} // namespace dachs

#endif    // DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED
