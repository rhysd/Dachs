#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_RESULT_OF_USE_DECLTYPE 1

// Include {{{
#include <string>
#include <memory>
#include <exception>
#include <vector>
#include <algorithm>
#include <utility>
#include <cstddef>

#include <boost/format.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_as.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include "parser.hpp"
#include "ast.hpp"
// }}}

namespace dachs {
namespace syntax {

// Import names {{{
namespace qi = boost::spirit::qi;
namespace spirit = boost::spirit;
namespace phx = boost::phoenix;
namespace ascii = boost::spirit::ascii;

using std::size_t;
using qi::_1;
using qi::_2;
using qi::_3;
using qi::_4;
using qi::_5;
using qi::_a;
using qi::_val;
using qi::lit;
// }}}

// Helpers {{{
namespace detail {
    template<class Iterator>
    inline auto line_pos_iterator(Iterator i)
    {
        return boost::spirit::line_pos_iterator<Iterator>(i);
    }

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

        sep = +(lit(';') ^ '\n');

        // FIXME: Temporary
        program
            = (
                (statement % sep) > -(sep) > (qi::eol | qi::eoi)
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
                | symbol_literal
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

        array_literal
            = (
                '[' >> -(
                    expression[phx::push_back(_a, _1)] % ','
                ) >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_literal>(_a)
            ];

        tuple_literal
            = (
                '(' >> -(
                    expression[phx::push_back(_a, _1)]
                    >> +(
                        ',' >> expression[phx::push_back(_a, _1)]
                    )
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_literal>(_a)
            ];

        symbol_literal
            = qi::lexeme[
                ':' >>
                qi::as_string[
                    +qi::char_
                ][
                    _val = make_node_ptr<ast::node::symbol_literal>(_1)
                ]
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

        parameter
            = (
                -qi::string("var")
                >> identifier
                >> -(
                    ':' >> type_name
                )
            ) [
                _val = make_node_ptr<ast::node::parameter>(_1, _2, _3)
            ];

        primary_expr
            = (
                literal | identifier | '(' >> expression >> ')'
            ) [
                _val = make_node_ptr<ast::node::primary_expr>(_1)
            ];

        index_access
            = (
                '[' >> expression >> ']'
            ) [
                _val = make_node_ptr<ast::node::index_access>(_1)
            ];

        member_access
            = (
                '.' >> identifier
            ) [
                _val = make_node_ptr<ast::node::member_access>(_1)
            ];

        function_call
            = (
                '(' >> -(
                    expression[phx::push_back(_a, _1)] % ','
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::function_call>(_a)
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
                -qi::string("maybe") >> identifier >> -(lit('[') >> ']')
            ) [
                _val = make_node_ptr<ast::node::type_name>(_1, _2)
            ];

        cast_expr
            = (
                unary_expr >> *("as" > type_name)
            ) [
                _val = make_node_ptr<ast::node::cast_expr>(_2, _1)
            ];

        mult_expr
            = (
                cast_expr >> *(
                    qi::as<ast::node_type::mult_expr::rhs_type>()[
                        mult_operator >> cast_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::mult_expr>(_1, _2)
            ];

        additive_expr
            = (
                mult_expr >> *(
                    qi::as<ast::node_type::additive_expr::rhs_type>()[
                        additive_operator >> mult_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::additive_expr>(_1, _2)
            ];

        shift_expr
            = (
                additive_expr >> *(
                    qi::as<ast::node_type::shift_expr::rhs_type>()[
                        shift_operator >> additive_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::shift_expr>(_1, _2)
            ];

        relational_expr
            = (
                shift_expr >> *(
                    qi::as<ast::node_type::relational_expr::rhs_type>()[
                        relational_operator >> shift_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::relational_expr>(_1, _2)
            ];

        equality_expr
            = (
                relational_expr >> *(
                    qi::as<ast::node_type::equality_expr::rhs_type>()[
                        equality_operator >> relational_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::equality_expr>(_1, _2)
            ];

        and_expr
            = (
                equality_expr >> *('&' >> equality_expr)
            ) [
                _val = make_node_ptr<ast::node::and_expr>(_1, _2)
            ];

        xor_expr
            = (
                and_expr >> *('^' >> and_expr)
            ) [
                _val = make_node_ptr<ast::node::xor_expr>(_1, _2)
            ];

        or_expr
            = (
                xor_expr >> *('|' >> xor_expr)
            ) [
                _val = make_node_ptr<ast::node::or_expr>(_1, _2)
            ];

        logical_and_expr
            = (
                or_expr >> *("&&" >> or_expr)
            ) [
                _val = make_node_ptr<ast::node::logical_and_expr>(_1, _2)
            ];

        logical_or_expr
            = (
                logical_and_expr >> *("||" >> logical_and_expr)
            ) [
                _val = make_node_ptr<ast::node::logical_or_expr>(_1, _2)
            ];

        if_expr
            = (
                if_kind >> (expression - "then") >> ("then" || sep)
                >> (expression - "else") >> -sep >> "else" >> -sep >> expression
            ) [
                _val = make_node_ptr<ast::node::if_expr>(_1, _2, _3, _4)
            ];

        expression
            = (
                (if_expr | logical_or_expr) >> -(':' >> type_name)
            ) [
                _val = make_node_ptr<ast::node::expression>(_1, _2)
            ];

        assignment_stmt
            = (
                postfix_expr % ',' >> assign_operator >> expression % ','
            ) [
                _val = make_node_ptr<ast::node::assignment_stmt>(_1, _2, _3)
            ];

        if_stmt
            = (
                if_kind >> (expression - "then") >> ("then" || sep)
                >> (statement - "end" - "elseif" - "else" - "then") % sep >> -sep
                >> *(
                    qi::as<ast::node_type::if_stmt::elseif_type>()[
                        "elseif" >> (expression - "then") >> ("then" || sep)
                        >> (statement - "end" - "elseif" - "else" - "then") % sep >> -sep
                    ]
                ) >> -("else" >> -sep >> (statement - "end") % sep >> -sep)
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::if_stmt>(_1, _2, _3, _4, _5)
            ];

        return_stmt
            = (
                "return" >> -(expression[phx::push_back(_a, _1)] % ',')
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(_a)
            ];

        case_stmt
            = (
                "case" >> sep
                >> +(
                    qi::as<ast::node_type::case_stmt::when_type>()[
                        "when" >> (expression - "then") >> ("then" || sep)
                        >> +((statement - "end" - "else") >> sep)
                    ]
                ) >> -(
                    "else" >> -sep >> (statement - "end") % sep >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::case_stmt>(_1, _2)
            ];

        switch_stmt
            = (
                "case" >> (expression - "when") >> sep
                >> +(
                    qi::as<ast::node_type::switch_stmt::when_type>()[
                        "when" >> (expression - "then") >> ("then" || sep)
                        >> +((statement - "end" - "else") >> sep)
                    ]
                ) >> -(
                    "else" >> -sep >> (statement - "end") % sep >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::switch_stmt>(_1, _2, _3)
            ];

        for_stmt
            = (
                // Note: "do" might colide with do-end block in expression
                "for" >> (parameter - "in") % ',' >> "in" >> expression >> ("do" || sep)
                >> (statement - "end") % sep >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::for_stmt>(_1, _2, _3)
            ];

        while_stmt
            = (
                // Note: "do" might colide with do-end block in expression
                "for" >> expression >> ("do" || sep)
                >> (statement - "end") % sep >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::while_stmt>(_1, _2)
            ];

        postfix_if_stmt
            = (
                (expression - if_kind) >> if_kind >> expression
            ) [
                _val = make_node_ptr<ast::node::postfix_if_stmt>(_1, _2, _3)
            ];

        statement
            = (
                  if_stmt
                | return_stmt
                | case_stmt
                | switch_stmt
                | for_stmt
                | while_stmt
                | assignment_stmt
                | postfix_if_stmt
                | expression
            ) [
                _val = make_node_ptr<ast::node::statement>(_1)
            ];

        qi::on_error<qi::fail>(
            program,
            // _1 : begin of string to parse
            // _2 : end of string to parse
            // _3 : iterator at failed point
            // _4 : what failed?
            std::cerr << phx::val("Syntax error at ")
                      << phx::bind([](auto const begin, auto const err_pos) {
                              return (boost::format("line:%1%, col:%2%") % spirit::get_line(err_pos) % spirit::get_column(begin, err_pos)).str();
                          }, _1, _3) << '\n'
                      << "expected " << qi::_4
                      << "\n\n"
                      << phx::bind([](auto const begin, auto const end, auto const err_itr) {
                              return std::string{
                                        spirit::get_line_start(begin, err_itr),
                                        std::find_if(err_itr, end, [](auto c){ return c == '\r' || c == '\n'; })
                                     } + '\n'
                                     + std::string(spirit::get_column(begin, err_itr)-1, ' ') + "↑ ここやで";
                          }, _1, _2, _3)
                      << '\n' << std::endl
        );
    }

    ~grammar()
    {}

private:

    // Rules {{{
    rule<qi::unused_type()> sep;

#define DACHS_DEFINE_RULE(n) rule<ast::node::n()> n;
#define DACHS_DEFINE_RULE_WITH_LOCALS(n, ...) rule<ast::node::n(), qi::locals< __VA_ARGS__ >> n;

    DACHS_DEFINE_RULE(program);
    DACHS_DEFINE_RULE(literal);
    DACHS_DEFINE_RULE(integer_literal);
    DACHS_DEFINE_RULE(character_literal);
    DACHS_DEFINE_RULE(float_literal);
    DACHS_DEFINE_RULE(boolean_literal);
    DACHS_DEFINE_RULE(string_literal);
    DACHS_DEFINE_RULE_WITH_LOCALS(array_literal, std::vector<ast::node::expression>);
    DACHS_DEFINE_RULE_WITH_LOCALS(tuple_literal, std::vector<ast::node::expression>);
    DACHS_DEFINE_RULE(symbol_literal);
    DACHS_DEFINE_RULE(identifier);
    DACHS_DEFINE_RULE(parameter);
    DACHS_DEFINE_RULE(primary_expr);
    DACHS_DEFINE_RULE(index_access);
    DACHS_DEFINE_RULE(member_access);
    DACHS_DEFINE_RULE_WITH_LOCALS(function_call, std::vector<ast::node::expression>);
    DACHS_DEFINE_RULE(postfix_expr);
    DACHS_DEFINE_RULE(unary_expr);
    DACHS_DEFINE_RULE(type_name);
    DACHS_DEFINE_RULE(cast_expr);
    DACHS_DEFINE_RULE(mult_expr);
    DACHS_DEFINE_RULE(additive_expr);
    DACHS_DEFINE_RULE(shift_expr);
    DACHS_DEFINE_RULE(relational_expr);
    DACHS_DEFINE_RULE(equality_expr);
    DACHS_DEFINE_RULE(and_expr);
    DACHS_DEFINE_RULE(xor_expr);
    DACHS_DEFINE_RULE(or_expr);
    DACHS_DEFINE_RULE(logical_and_expr);
    DACHS_DEFINE_RULE(logical_or_expr);
    DACHS_DEFINE_RULE(if_expr);
    DACHS_DEFINE_RULE(expression);
    DACHS_DEFINE_RULE(if_stmt);
    DACHS_DEFINE_RULE_WITH_LOCALS(return_stmt, std::vector<ast::node::expression>);
    DACHS_DEFINE_RULE(case_stmt);
    DACHS_DEFINE_RULE(switch_stmt);
    DACHS_DEFINE_RULE(for_stmt);
    DACHS_DEFINE_RULE(while_stmt);
    DACHS_DEFINE_RULE(assignment_stmt);
    DACHS_DEFINE_RULE(postfix_if_stmt);
    DACHS_DEFINE_RULE(statement);

#undef DACHS_DEFINE_RULE
#undef DACHS_DEFINE_RULE_WITH_LOCALS
    // }}}

    // Symbol tables {{{
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
                (">>", ast::shift_operator::left)
                ("<<", ast::shift_operator::right)
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

    struct if_kind_rule_type : public qi::symbols<char, ast::if_kind> {
        if_kind_rule_type()
        {
            add
                ("if", ast::if_kind::if_)
                ("unless", ast::if_kind::unless)
            ;
        }
    } if_kind;
    // }}}
};

ast::ast parser::parse(std::string const& code)
{
    auto itr = detail::line_pos_iterator(std::begin(code));
    auto const begin = itr;
    auto const end = detail::line_pos_iterator(std::end(code));
    ast::node::program root;

    if (!qi::phrase_parse(itr, end, spiritual_parser, ascii::blank, root) || itr != end) {
        throw parse_error{spirit::get_line(itr), spirit::get_column(begin, itr)};
    }

    return {root};
}

} // namespace syntax
} // namespace dachs
