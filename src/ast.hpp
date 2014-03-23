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

namespace node_type {

std::size_t generate_id();

// Forward class declarations
struct integer_literal;
struct array_literal;
struct tuple_literal;
struct literal;
struct program;

}

namespace node {

using integer_literal = std::shared_ptr<node_type::integer_literal>;
using array_literal = std::shared_ptr<node_type::array_literal>;
using tuple_literal = std::shared_ptr<node_type::tuple_literal>;
using literal = std::shared_ptr<node_type::literal>;
using program = std::shared_ptr<node_type::program>;

} // namespace node

namespace node_type {

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

    std::string to_string() const override
    {
        return "INTEGER_LITERAL: " + boost::lexical_cast<std::string>(value);
    }
};

struct array_literal : public base {
    // TODO: Not implemented
    std::string to_string() const override
    {
        return "ARRAY_LITERAL (Not implemented)";
    }
};

struct tuple_literal : public base {
    // TODO: Not implemented
    std::string to_string() const override
    {
        return "TUPLE_LITERAL (Not implemented)";
    }
};

struct literal : public base {
    using value_type =
        boost::variant< char
                      , double
                      , std::string
                      , node::integer_literal
                      , node::array_literal
                      , node::tuple_literal >;
    value_type value;

    template<class T>
    explicit literal(T && v)
        : base(), value(std::forward<T>(v))
    {}

    std::string to_string() const override
    {
        std::string const kind
            = helper::variant::has<char>(value) ? "char" :
              helper::variant::has<double>(value) ? "float" :
              helper::variant::has<std::string>(value) ? "string" :
              helper::variant::has<node::integer_literal>(value) ? "integer" :
              helper::variant::has<node::array_literal>(value) ? "array" : "tuple";
        return "LITERAL: " + kind;
    }
};

struct program : public base {
    node::literal value; // TEMPORARY

    program(node::literal const& lit)
        : base(), value(lit)
    {}

    std::string to_string() const override
    {
        return "PROGRAM";
    }
};

} // namespace node_type

struct ast {
    node::program root;
};

} // namespace ast
} // namespace syntax
} // namespace dachs

#endif    // DACHS_AST_HPP_INCLUDED
