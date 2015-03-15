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

        for (auto const& f : program->functions) {

            // Note:
            // Here, 'f' may returns true even if the 'main' function
            // is member function and is not entry point. However, such
            // a member function is rare case and importing std.argv is
            // not harmful.  So I decided to import std.argv if the function
            // 'seems' main function.
            if (f->is_main_func()) {
                if (!f->params.empty()) {
                    program->imports.push_back(
                                helper::make<ast::node::import>("std.argv")
                            );
                }
                break;
            }
        }
    }
};

} // namespace syntax
} // namespace dachs

#endif    // DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED
