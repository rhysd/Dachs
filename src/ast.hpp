#if !defined DACHS_AST_HPP_INCLUDED
#define      DACHS_AST_HPP_INCLUDED

#include <string>
#include <cstddef>
#include <memory>
#include <boost/variant.hpp>
#include <boost/lexical_cast.hpp>

#include "helper/variant.hpp"

namespace dachs {
namespace syntax {
namespace ast {

namespace node {

std::size_t generate_id();

struct base {
    base()
        : id(generate_id())
    {}

    virtual ~base()
    {}

    virtual std::string to_string() const = 0;

    std::size_t line, col, length;
    std::size_t id;
};

struct integer_literal : public base {
    boost::variant< int
                  , unsigned int
    > value;

    template<class T>
    explicit integer_literal(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const
    {
        return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value);
    }
};

struct array_literal : public base {
    // TODO: Not implemented
    std::string to_string() const
    {
        return "ARRAY_LITERAL: ";
    }
};

struct literal : public base {
    using value_type =
        boost::variant< char
                      , double
                      , std::string
                      , std::shared_ptr<integer_literal>
                      , std::shared_ptr<array_literal> >;
    value_type value;

    template<class T>
    explicit literal(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const
    {
        std::string const kind
            = helper::variant::has<char>(value) ? "char" :
              helper::variant::has<double>(value) ? "float" :
              helper::variant::has<std::string>(value) ? "string" :
              helper::variant::has<std::shared_ptr<integer_literal>>(value) ? "integer" : "array";
        return "LITERAL: " + kind;
    }
};

struct program : public base {
    std::shared_ptr<literal> value; // TEMPORARY

    std::string to_string() const
    {
        return "PROGRAM";
    }
};

} // namespace node

struct ast {
    std::shared_ptr<node::program> root;
};

} // namespace ast
} // namespace syntax
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
