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

namespace dachs {
namespace type_node {
struct builtin_type;
struct class_type;
struct template_holder_type;
struct class_template_type;
struct tuple_type;
struct func_type;
struct proc_type;
struct dict_type;
struct array_type;
struct dict_type;
struct qualified_type;
}

namespace type {
#define DACHS_DEFINE_TYPE(n) \
   using n = std::shared_ptr<type_node::n>; \
   using weak_##n = std::weak_ptr<type_node::n>
DACHS_DEFINE_TYPE(builtin_type);
DACHS_DEFINE_TYPE(class_type);
DACHS_DEFINE_TYPE(template_holder_type);
DACHS_DEFINE_TYPE(class_template_type);
DACHS_DEFINE_TYPE(tuple_type);
DACHS_DEFINE_TYPE(func_type);
DACHS_DEFINE_TYPE(proc_type);
DACHS_DEFINE_TYPE(dict_type);
DACHS_DEFINE_TYPE(array_type);
DACHS_DEFINE_TYPE(dict_type);
DACHS_DEFINE_TYPE(qualified_type);
#undef DACHS_DEFINE_TYPE

using any_type
    = boost::variant< builtin_type
                    , class_type
                    , template_holder_type
                    , class_template_type
                    , tuple_type
                    , func_type
                    , proc_type
                    , dict_type
                    , array_type
                    , dict_type
                    , qualified_type
                >;

using type = any_type ; // For external use

template<class Type, class... Args>
inline Type make(Args &&... args)
{
    return std::make_shared<typename Type::element_type>(std::forward<Args>(args)...);
}

// Considering about the ability to add more qualifiers
enum class qualifier {
    maybe,
};

} // namespace type

namespace type_node {

using boost::algorithm::join;
using boost::adaptors::transformed;

namespace detail {
struct to_string : public boost::static_visitor<std::string> {
    template<class Type>
    std::string operator()(Type const& t) const
    {
        return t->to_string();
    }
};
} // namespace detail

struct basic_type {
    virtual std::string to_string() const = 0;
    virtual ~basic_type()
    {}
};

struct builtin_type final : public basic_type {
    std::string name;

    std::string to_string() const override
    {
        return name;
    }
};

struct class_type final : public basic_type {
    std::string name;

    std::string to_string() const override
    {
        return name;
    }
};

struct template_holder_type final : public basic_type {
    std::string name;

    std::string to_string() const override
    {
        return name;
    }
};

struct class_template_type final : public basic_type {
    std::string name;
    std::vector<type::any_type> holder_types;

    std::string to_string() const override
    {
        return name + '(' +
            join(holder_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct tuple_type final : public basic_type {
    std::vector<type::any_type> element_types;

    std::string to_string() const override
    {
        return '(' +
            join(element_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct func_type final : public basic_type {
    std::string name;
    std::vector<type::any_type> param_types;
    // If return type is not specified, it will be treated as template type
    type::any_type return_type;

    std::string to_string() const override
    {
        auto const maybe_template_holder = helper::variant::get<type::template_holder_type>(return_type);
        return name + '(' +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')'
            + (maybe_template_holder ?  (*maybe_template_holder)->to_string() : std::string{});
    }
};

struct proc_type final : public basic_type {
    std::string name;
    std::vector<type::any_type> param_types;

    std::string to_string() const override
    {
        return name + '(' +
            join(param_types | transformed([](auto const& t){
                        return boost::apply_visitor(detail::to_string{}, t);
                    }), ",")
            + ')';
    }
};

struct dict_type final : public basic_type {
    type::any_type key_type, value_type;

    std::string to_string() const override
    {
        detail::to_string v;
        return '{'
            + boost::apply_visitor(v, key_type) + " => "
            + boost::apply_visitor(v, value_type)
            + '}';
    }
};

struct array_type final : public basic_type {
    type::any_type element_type;

    std::string to_string() const override
    {
        return '{'
            + boost::apply_visitor(detail::to_string{}, element_type)
            + '}';
    }
};

struct qualified_type final : public basic_type {
    type::qualifier qualifier;
    type::any_type contained_type;

    std::string to_string() const override
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
