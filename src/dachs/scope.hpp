#if !defined DACHS_SCOPE_HPP_INCLUDED
#define      DACHS_SCOPE_HPP_INCLUDED

#include <memory>

#include "dachs/ast.hpp"
#include "dachs/symbol.hpp"

namespace dachs {

// Forward declarations to define node pointers
namespace scope_node {

struct global_scope;
struct local_scope;

} // namespace scope

// Dynamic resources to use actually
namespace scope {

using global_scope = std::shared_ptr<scope_node::global_scope>;
using local_scope = std::shared_ptr<scope_node::local_scope>;

} // namespace scope

// Implementation of nodes of scope tree
namespace scope_node {

struct basic_scope {
    virtual ~basic_scope()
    {}
};

struct global_scope final : public basic_scope {};

struct local_scope final : public basic_scope {};



} // namespace scope_node

namespace scope {

struct scope_tree {
    scope::global_scope root;
};

} // namespace scope
} // namespace dachs

#endif    // DACHS_SCOPE_HPP_INCLUDED
