#include <cstddef>
#include <iostream>

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/numeric.hpp>
#include <boost/range/empty.hpp>

#include "dachs/helper/stringize_scope_tree.hpp"

namespace dachs {
namespace helper {

namespace detail {

using boost::adaptors::transformed;
using std::size_t;

class scope_tree_stringizer {

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
        return boost::accumulate(symbols | transformed([this, i, &prefix](auto const& s){ return indent(i) + prefix + s->name; }),
                          std::string{},
                          [](auto const& acc, auto const& item){
                              return acc + '\n' + item;
                          });
    }

public:

    std::string visit(scope::local_scope const& l, size_t const i) const
    {
        return indent(i) + "LOCAL_SCOPE"
            + visit_symbols(l->local_vars, i+1, "DEF: ")
            + visit_scopes(l->children, i+1);
    }

    std::string visit(scope::func_scope const& f, size_t const i) const
    {
        return indent(i) + "FUNCTION_SCOPE: " + f->name
            + visit_symbols(f->params, i+1, "DEF: ")
            + '\n' + visit(f->body, i+1);
    }

    std::string visit(scope::global_scope const& g, size_t const i) const
    {
        return indent(i) + "GLOBAL_SCOPE"
            + visit_symbols(g->const_symbols, i+1, "DEF: ")
            + visit_scopes(g->functions, i+1)
            + visit_scopes(g->classes, i+1);
    }

    std::string visit(scope::class_scope const& c, size_t const i) const
    {
        return indent(i) + "CLASS_SCOPE: " + c->name
            + visit_symbols(c->member_var_symbols, i+1, "DEF: ")
            + visit_scopes(c->member_func_scopes, i+1)
            + visit_scopes(c->inherited_class_scopes, i+1);
    }
};

} // namespace detail

std::string stringize_scope_tree(scope::scope_tree const& tree)
{
    return detail::scope_tree_stringizer{}.visit(tree.root, 0);
}

} // namespace helper
} // namespace dachs

