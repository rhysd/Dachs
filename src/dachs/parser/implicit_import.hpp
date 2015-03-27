#if !defined DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED
#define      DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED

#include <algorithm>
#include <boost/range/algorithm/find_if.hpp>

#include "dachs/ast/ast.hpp"

namespace dachs {
namespace syntax {

struct implicit_import {
    bool range_expr_found = false;
    bool array_found = false;
    bool string_found = false;

    void install(ast::node::inu const& program) const
    {
        auto const import_if
            = [&program](bool const cond, char const* const module)
            {
                if (cond) {
                    program->imports.push_back(
                            ast::make<ast::node::import>(module)
                        );
                }
            };

        import_if(range_expr_found, "std.range");
        import_if(array_found, "std.array");
        import_if(string_found, "std.string");

        auto const main_func_itr
            = boost::find_if(
                    program->functions,
                    [](auto const& f)
                    {
                        return f->is_main_func() && !f->params.empty();
                    }
                );

        // Note:
        // Here, 'f' may returns true even if the 'main' function
        // is member function and is not entry point. However, such
        // a member function is rare case and importing std.argv is
        // not harmful.  So I decided to import std.argv if the function
        // 'seems' main function.
        import_if(main_func_itr != boost::end(program->functions), "std.argv");
    }
};

} // namespace syntax
} // namespace dachs

#endif    // DACHS_PARSER_IMPLICIT_IMPORT_HPP_INCLUDED
