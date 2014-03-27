#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_RESULT_OF_USE_DECLTYPE 1

#include <string>
#include <memory>
#include <exception>
#include <cstddef>
#include <utility>
#include <vector>

#include <boost/format.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_as.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>

#include "parser.hpp"
#include "ast.hpp"

namespace dachs {
namespace syntax {

using std::size_t;

namespace detail {
    template<class Iterator>
    auto position_of(Iterator begin, Iterator current)
    {
        size_t line = 1;
        size_t col = 1;
        for (; begin != current; ++begin) {
            if (*begin == '\n') {
                col = 1;
                line++;
                continue;
            }
            col++;
        }
        return std::make_pair(line, col);
    }
} // namespace detail

namespace qi = boost::spirit::qi;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;

using qi::_1;
using qi::_2;
using qi::_3;
using qi::_val;
using qi::lit;
using phx::bind;

// Helpers {{{
namespace detail {
    template<class Node>
    struct make_shared {
        template<class... Args>
        auto operator()(Args &&... args) const
        {
            return std::make_shared<Node>(std::forward<Args>(args)...);
        }
    };
} // namespace detail

template<class NodeType, class... Holders>
inline auto make_node_ptr(Holders &&... holders)
{
    return phx::bind(detail::make_shared<typename NodeType::element_type>{}, std::forward<Holders>(holders)...);
}
// }}}

template<class Iterator>
class grammar : public qi::grammar<Iterator, ast::node::program(), ascii::blank_type /*FIXME*/> {
    template<class Value, class... Extra>
    using rule = qi::rule<Iterator, Value, ascii::blank_type, Extra...>;

public:
    grammar() : grammar::base_type(program)
    {
        // FIXME: Temporary
        program
            = (
                cast_expr > (qi::eol | qi::eoi)
            ) [
                _val = make_node_ptr<ast::node::program>(_1)
            ];

        literal
            = (
                  character_literal
                | string_literal
                | boolean_literal
                | float_literal
                | integer_literal
                | array_literal
                | tuple_literal
            ) [
                _val = make_node_ptr<ast::node::literal>(_1)
            ];

        character_literal
            = (
                '\'' > qi::char_ > '\''
            ) [
                _val = make_node_ptr<ast::node::character_literal>(_1)
            ];

        float_literal
            = (
                (qi::real_parser<double, qi::strict_real_policies<double>>())
            ) [
                _val = make_node_ptr<ast::node::float_literal>(_1)
            ];

        boolean_literal
            = (
                qi::bool_
            ) [
                _val = make_node_ptr<ast::node::boolean_literal>(_1)
            ];

        string_literal
            = (
                qi::as_string[qi::lexeme['"' > *(qi::char_ - '"') > '"']]
            ) [
                _val = make_node_ptr<ast::node::string_literal>(_1)
            ];

        integer_literal
            = (
                  (qi::lexeme[qi::uint_ >> 'u']) | qi::int_
            ) [
                _val = make_node_ptr<ast::node::integer_literal>(_1)
            ];

        // FIXME: Temporary
        array_literal
            = (
                lit('[') >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_literal>()
            ];

        // FIXME: Temporary
        tuple_literal
            = (
                '(' >> +lit(',') >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_literal>()
            ];

        identifier
            = (
                qi::as_string[
                    qi::lexeme[
                        (qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_'))
                    ]
                ]
            ) [
                _val = make_node_ptr<ast::node::identifier>(_1)
            ];

        primary_expr
            = (
                literal | identifier // | '(' >> expr >> ')'
            ) [
                _val = make_node_ptr<ast::node::primary_expr>(_1)
            ];

        index_access
            = (
                lit('[') >> /* -expression >> */ ']' // TODO: Temporary
            ) [
                _val = make_node_ptr<ast::node::index_access>()
            ];

        member_access
            = (
                '.' >> identifier
            ) [
                _val = make_node_ptr<ast::node::member_access>(_1)
            ];

        function_call
            = (
                '(' >> qi::eps >> ')' // TODO: Temporary
            ) [
                _val = make_node_ptr<ast::node::function_call>()
            ];

        postfix_expr
            = (
                  primary_expr >> *(member_access | index_access | function_call)
            ) [
                _val = make_node_ptr<ast::node::postfix_expr>(_1, _2)
            ];

        unary_expr
            = (
                *unary_operator >> postfix_expr
            ) [
                _val = make_node_ptr<ast::node::unary_expr>(_1, _2)
            ];

        type_name
            = (
                identifier >> -(lit('[') >> ']')
            ) [
                _val = make_node_ptr<ast::node::type_name>(_1)
            ];

        cast_expr
            = (
                unary_expr >> *("as" > type_name)
            ) [
                _val = make_node_ptr<ast::node::cast_expr>(_2, _1)
            ];

        qi::on_error<qi::fail>(
            program,
            // qi::_1 : begin of string to parse
            // qi::_2 : end of string to parse
            // qi::_3 : iterator at failed point
            // qi::_4 : what failed?
            std::cerr << phx::val("Syntax error at ")
                      << bind([](auto const begin, auto const err_pos) -> std::string {
                              auto const pos = detail::position_of(begin, err_pos);
                              return (boost::format("line:%1%, col:%2%") % pos.first % pos.second).str();
                          }, _1, _3) << '\n'
                      << "expected " << qi::_4
                      << "\n\n"
                      << bind([](auto const begin, auto const end, auto const err_itr) -> std::string {
                              auto const pos = detail::position_of(begin, err_itr);
                              std::vector<std::string> lines;
                              auto const r = boost::make_iterator_range(begin, end);
                              boost::algorithm::split(lines, r, [](char const c){ return c == '\n'; });
                              return lines[pos.first-1] + '\n' + std::string(pos.second-1, ' ') + "↑ ここやで";
                          }, _1, _2, _3)
                      << std::endl
        );
    }

    ~grammar()
    {}

private:
#define DACHS_PARSE_RULE(n) rule<ast::node::n()> n;

    DACHS_PARSE_RULE(program);
    DACHS_PARSE_RULE(literal);
    DACHS_PARSE_RULE(integer_literal);
    DACHS_PARSE_RULE(character_literal);
    DACHS_PARSE_RULE(float_literal);
    DACHS_PARSE_RULE(boolean_literal);
    DACHS_PARSE_RULE(string_literal);
    DACHS_PARSE_RULE(array_literal);
    DACHS_PARSE_RULE(tuple_literal);
    DACHS_PARSE_RULE(identifier);
    DACHS_PARSE_RULE(primary_expr);
    DACHS_PARSE_RULE(index_access);
    DACHS_PARSE_RULE(member_access);
    DACHS_PARSE_RULE(function_call);
    DACHS_PARSE_RULE(postfix_expr);
    DACHS_PARSE_RULE(unary_expr);
    DACHS_PARSE_RULE(type_name);
    DACHS_PARSE_RULE(cast_expr);

#undef DACHS_PARSE_RULE

    //
    // Symbol tables
    //
    struct unary_operator_rule_type : public qi::symbols<char, ast::unary_operator> {
        unary_operator_rule_type()
        {
            add
                ("+", ast::unary_operator::positive)
                ("-", ast::unary_operator::negative)
                ("~", ast::unary_operator::one_complement)
                ("!", ast::unary_operator::logical_negate)
            ;
        }
    } unary_operator;

    struct mult_operator_rule_type : public qi::symbols<char, ast::mult_operator> {
        mult_operator_rule_type()
        {
            add
                ("*", ast::mult_operator::mult)
                ("/", ast::mult_operator::div)
                ("%", ast::mult_operator::mod)
            ;
        }
    } mult_operator;

    struct additive_operator_rule_type : public qi::symbols<char, ast::additive_operator> {
        additive_operator_rule_type()
        {
            add
                ("+", ast::additive_operator::add)
                ("-", ast::additive_operator::sub)
            ;
        }
    } additive_operator;

    struct relational_operator_rule_type : public qi::symbols<char, ast::relational_operator> {
        relational_operator_rule_type()
        {
            add
                ("<", ast::relational_operator::less_than)
                (">", ast::relational_operator::greater_than)
                ("<=", ast::relational_operator::less_than_equal)
                (">=", ast::relational_operator::greater_than_equal)
            ;
        }
    } relational_operator;

    struct shift_operator_rule_type : public qi::symbols<char, ast::shift_operator> {
        shift_operator_rule_type()
        {
            add
                ("+", ast::shift_operator::left)
                ("-", ast::shift_operator::right)
            ;
        }
    } shift_operator;

    struct equality_operator_rule_type : public qi::symbols<char, ast::equality_operator> {
        equality_operator_rule_type()
        {
            add
                ("==", ast::equality_operator::equal)
                ("!=", ast::equality_operator::not_equal)
            ;
        }
    } equality_operator;

    struct assign_operator_rule_type : public qi::symbols<char, ast::assign_operator> {
        assign_operator_rule_type()
        {
            add
                ("=", ast::assign_operator::assign)
                ("*=", ast::assign_operator::mult)
                ("/=", ast::assign_operator::div)
                ("%=", ast::assign_operator::mod)
                ("+=", ast::assign_operator::add)
                ("-=", ast::assign_operator::sub)
                ("<<=", ast::assign_operator::left_shift)
                (">>=", ast::assign_operator::right_shift)
                ("&=", ast::assign_operator::arithmetic_and)
                ("^=", ast::assign_operator::arithmetic_xor)
                ("|=", ast::assign_operator::arithmetic_or)
                ("&&=", ast::assign_operator::logical_and)
                ("||=", ast::assign_operator::logical_or)
            ;
        }
    } assign_operator;

};


ast::ast parser::parse(std::string const& code)
{
    auto itr = std::begin(code);
    auto const end = std::end(code);
    grammar<decltype(itr)> spiritual_parser;
    ast::node::program root;

    if (!qi::phrase_parse(itr, end, spiritual_parser, ascii::blank, root) || itr != end) {
        throw parse_error{detail::position_of(std::begin(code), itr)};
    }

    return {root};
}

} // namespace syntax
} // namespace dachs
