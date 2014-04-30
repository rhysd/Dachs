#if !defined DACHS_SCOPE_FWD_HPP_INCLUDED
#define      DACHS_SCOPE_FWD_HPP_INCLUDED

#include <memory>

#include <boost/variant/variant.hpp>

#include "dachs/symbol.hpp"

namespace dachs {

// Forward declarations to define node pointers
namespace scope_node {

struct global_scope;
struct local_scope;
struct func_scope;
struct class_scope;

} // namespace scope

namespace scope {

#define DACHS_DEFINE_SCOPE(s) \
   using s = std::shared_ptr<scope_node::s>; \
   using weak_##s = std::weak_ptr<scope_node::s>
DACHS_DEFINE_SCOPE(global_scope);
DACHS_DEFINE_SCOPE(local_scope);
DACHS_DEFINE_SCOPE(func_scope);
DACHS_DEFINE_SCOPE(class_scope);
#undef DACHS_DEFINE_SCOPE

using any_scope
        = boost::variant< global_scope
                        , local_scope
                        , func_scope
                        , class_scope
                    >;

using enclosing_scope_type
        = boost::variant< weak_global_scope
                        , weak_local_scope
                        , weak_func_scope
                        , weak_class_scope
                    >;

} // namespace scope

namespace symbol {

using weak_type_symbol
        = boost::variant< weak_builtin_type_symbol
                        , weak_template_type_symbol
                        , scope::class_scope
                    >;

} // namespace symbol

} // namespace dachs

#endif    // DACHS_SCOPE_FWD_HPP_INCLUDED
