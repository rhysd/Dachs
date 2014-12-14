#include <cstddef>
#include <iostream>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/numeric.hpp>
#include <boost/range/empty.hpp>

#include "dachs/semantics/stringize_scope_tree.hpp"
#include "dachs/helper/colorizer.hpp"

namespace dachs {
namespace scope {

namespace detail {

using boost::adaptors::transformed;
using std::size_t;

class scope_tree_stringizer {

    helper::colorizer c;

    std::string indent(size_t const i) const
    {
        return std::string(i*2, ' ');
    }

    template<class Range>
    std::string visit_scopes(Range const& scopes, size_t const i) const
    {
        if (boost::empty(scopes)) {
            return "";
        }
        return boost::accumulate(scopes | transformed([this, i](auto const& s){ return visit(s, i); }),
                          std::string{},
                          [](auto const& acc, auto const& item){
                              return acc + '\n' + item;
                          });
    }

    template<class Range>
    std::string visit_symbols(Range const& symbols, size_t const i, char const* const prefix) const
    {
        if (boost::empty(symbols)) {
            return "";
        }
        return boost::accumulate(symbols | transformed([this, i, &prefix](auto const& s){ return indent(i) + c.yellow(prefix) + s->name + (s->type ? ": " + c.cyan(s->type.to_string()) : ""); }),
                          std::string{},
                          [](auto const& acc, auto const& item){
                              return acc + '\n' + item;
                          });
    }

public:

    std::string visit(local_scope const& l, size_t const i) const
    {
        return indent(i) + c.green("LOCAL_SCOPE")
            + visit_symbols(l->local_vars, i+1, "SYMBOL: ")
            + visit_scopes(l->children, i+1)
            + visit_scopes(l->unnamed_funcs, i+1);
    }

    std::string visit(func_scope const& f, size_t const i) const
    {
        return indent(i) + c.green("FUNCTION_SCOPE: ") + f->to_string()
            + visit_symbols(f->params, i+1, "SYMBOL: ")
            + '\n' + visit(f->body, i+1);
    }

    std::string visit(global_scope const& g, size_t const i) const
    {
        return indent(i) + c.green("GLOBAL_SCOPE")
            + visit_symbols(g->const_symbols, i+1, "SYMBOL: ")
            + visit_scopes(g->functions, i+1)
            + visit_scopes(g->classes, i+1);
    }

    std::string visit(class_scope const& cl, size_t const i) const
    {
        return indent(i) + c.green("CLASS_SCOPE: ") + cl->name
            + visit_symbols(cl->member_var_symbols, i+1, "SYMBOL: ")
            + visit_scopes(cl->member_func_scopes, i+1);
    }
};

} // namespace detail

std::string stringize_scope_tree(scope_tree const& tree)
{
    return detail::scope_tree_stringizer{}.visit(tree.root, 0);
}

} // namespace scope
} // namespace dachs

