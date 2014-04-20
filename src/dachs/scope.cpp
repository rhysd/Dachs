#include <boost/variant/apply_visitor.hpp>
#include <boost/variant/static_visitor.hpp>

#include "dachs/scope.hpp"
#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"

namespace dachs {
namespace scope {

namespace detail {

// AST walker to generate a scope tree
struct scope_tree_generator {

    enclosing_scope_type current_scope;

    global_scope visit(ast::node::program &p)
    {
        auto this_scope = make<global_scope>();
        current_scope = this_scope;

        struct def_visitor_ : public boost::static_visitor<void>{
            global_scope &parent;
            scope_tree_generator *generator;

            def_visitor_(global_scope &p, scope_tree_generator *g)
                : parent(p), generator(g)
            {}

            void operator()(ast::node::function_definition &def)
            {
                auto func = make<func_scope>(parent, def->name->value);
                // TODO: visit function scope by generator
                parent->define_function(func);
            }

            void operator()(ast::node::procedure_definition &def)
            {
                auto proc = make<func_scope>(parent, def->name->value);
                // TODO: visit procedure scope by generator
                parent->define_function(proc);
            }

            void operator()(ast::node::constant_definition const& def)
            {
                for (auto const& const_decl : def->const_decls) {
                    parent->define_global_constant(
                                symbol::make<symbol::var_symbol>(const_decl->name->value)
                            );
                }
            }

            // TODO: get class definition

        } def_visitor{this_scope, this};

        for( auto &def : p->inu ) {
            boost::apply_visitor(def_visitor, def->value);
        }
        return this_scope;
    }
};

} // namespace detail

scope_tree make_scope_tree(ast::ast &a)
{
    return scope_tree{detail::scope_tree_generator{}.visit(a.root)};
}

std::string dump_scope_tree(scope_tree const&)
{
    return ""; // TODO
}

} // namespace scope
} // namespace dachs
