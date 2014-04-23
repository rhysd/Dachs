#if !defined DACHS_SCOPE_FWD_HPP_INCLUDED
#define      DACHS_SCOPE_FWD_HPP_INCLUDED

#include <memory>

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

} // namespace scope

} // namespace dachs

#endif    // DACHS_SCOPE_FWD_HPP_INCLUDED
