#if !defined DACHS_TYPE_HPP_INCLUDED
#define      DACHS_TYPE_HPP_INCLUDED

#include <string>
#include <memory>
#include <vector>
#include <cassert>

#include <boost/variant/variant.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/variant/apply_visitor.hpp>

#include "dachs/helper/variant.hpp"
#include "dachs/helper/make.hpp"

namespace dachs {
namespace type_node {
struct primary_class_type;
struct template_holder_type;
struct class_template_type;
struct func_type;
struct proc_type;
struct qualified_type;
}

namespace type {
#define DACHS_DEFINE_TYPE(n) \
   using n = std::shared_ptr<type_node::n>; \
   using weak_##n = std::weak_ptr<type_node::n>
DACHS_DEFINE_TYPE(primary_class_type);
DACHS_DEFINE_TYPE(template_holder_type);
DACHS_DEFINE_TYPE(class_template_type);
DACHS_DEFINE_TYPE(func_type);
DACHS_DEFINE_TYPE(proc_type);
DACHS_DEFINE_TYPE(qualified_type);
#undef DACHS_DEFINE_TYPE

using any_type
    = boost::variant< primary_class_type
                    , template_holder_type
                    , class_template_type
                    , func_type
                    , proc_type
                    , qualified_type
                >;

using type = any_type ; // For external use

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

using dachs::helper::make;

} // namespace type

namespace type_node {

using boost::algorithm::join;
using boost::adaptors::transformed;

namespace detail {
struct to_string : public boost::static_visitor<std::string> {
    template<class Type>
    std::string operator()(Type const& t) const noexcept
    {
        return t->to_string();
    }
};
} // namespace detail

struct basic_type {
    std::string name;

    explicit basic_type(std::string && name) noexcept
        : name(std::forward<std::string>(name))
    {}

    explicit basic_type(std::string const& name) noexcept
        : name(name)
    {}

    virtual std::string to_string() const noexcept = 0;
    virtual ~basic_type()
    {}
};

struct primary_class_type final : public basic_type {
    using basic_type::basic_type;
    std::string to_string() const noexcept override
    {
        return name;
    }
};

struct template_holder_type final : public basic_type {
    using basic_type::basic_type;
    std::string to_string() const noexcept override
    {
        return name;
    }
};

// This class may not be needed because class from class template is instanciated at the point on resolving a symbol of class templates
struct class_template_type final : public basic_type {
    std::vector<type::any_type> holder_types;

    using basic_type::basic_type;

    std::string to_string() const noexcept override
    {

        if (name == "tuple" ) {
            return '(' +
                join(holder_types | transformed([](auto const& t){
                            return boost::apply_visitor(detail::to_string{}, t);
                        }), ",")
                + ')';
        } else if (name == "array") {
            assert(holder_types.size() == 1);
            return '[' + boost::apply_visitor(detail::to_string{}, holder_types[0]) + ']';
        } else if (name == "dict") {
            assert(holder_types.size() == 2);
            return '{'
                + boost::apply_visitor(detail::to_string{}, holder_types[0])
                + " => "
                + boost::apply_visitor(detail::to_string{}, holder_types[1])
                + '}';
        } else {
            return name + '(' +
                join(holder_types | transformed([](auto const& t){
                            return boost::apply_visitor(detail::to_string{}, t);
                        }), ",")
                + ')';
        }
    }
};

struct func_type final : public basic_type {
    std::vector<type::any_type> param_types;
    // If return type is not specified, it will be treated as template type
    type::any_type return_type;

    func_type() noexcept
        : basic_type("func")
    {}

    explicit func_type(std::vector<type::any_type> const& params, type::any_type ret) noexcept
        : basic_type("func"), param_types(params), return_type(ret)
    {}

    std::string to_string() const noexcept override
    {
        auto const maybe_template_holder = helper::variant::get<type::template_holder_type>(return_type);
        return "func " + name + '(' +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')'
            + (maybe_template_holder ?  (*maybe_template_holder)->to_string() : std::string{});
    }
};

struct proc_type final : public basic_type {
    std::vector<type::any_type> param_types;

    proc_type() noexcept
        : basic_type("func")
    {}

    explicit proc_type(std::vector<type::any_type> const& params) noexcept
        : basic_type("func"), param_types(params)
    {}


    std::string to_string() const noexcept override
    {
        return name + '(' +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct qualified_type final : public basic_type {
    type::qualifier qualifier;
    type::any_type contained_type;

    qualified_type() noexcept
        : basic_type("maybe")
    {}

    std::string to_string() const noexcept override
    {
        auto const contained_type_name =  boost::apply_visitor(detail::to_string{}, contained_type);
        switch (qualifier) {
        case type::qualifier::maybe:
            return contained_type_name + '?';
        default:
            assert(false);
        }
    }
};

} // namespace type_node

} // namespace dachs

#endif    // DACHS_TYPE_HPP_INCLUDED
