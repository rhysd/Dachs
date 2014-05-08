#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_RESULT_OF_USE_DECLTYPE 1

// Include {{{
#include <string>
#include <memory>
#include <exception>
#include <vector>
#include <algorithm>
#include <utility>
#include <type_traits>
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

#include "dachs/parser.hpp"
#include "dachs/comment_skipper.hpp"
#include "dachs/ast.hpp"
#include "dachs/exception.hpp"
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

    template<class Getter>
    void set_position_getter_on_success(Getter const&)
    {}

    template<class Getter, class RulesHead, class... RulesTail>
    void set_position_getter_on_success(Getter const& getter, RulesHead& head, RulesTail &... tail)
    {
        qi::on_success(head, getter);
        detail::set_position_getter_on_success(getter, tail...);
    }
} // namespace detail

template<class NodeType, class... Holders>
inline auto make_node_ptr(Holders &&... holders)
{
    return phx::bind(detail::make_shared<typename NodeType::element_type>{}, std::forward<Holders>(holders)...);
}

template<class Holder>
inline auto as_vector(Holder && h)
{
    return phx::bind([](auto const& optional_vector)
                -> typename std::decay<decltype(*optional_vector)>::type {
                if (optional_vector) {
                    return *optional_vector;
                } else {
                    return {};
                }
            }, std::forward<Holder>(h));
}

// }}}

template<class FloatType>
struct strict_real_policies_disallowing_trailing_dot final
    : qi::strict_real_policies<FloatType> {
    static bool const allow_trailing_dot = false;
    // static bool const allow_leading_dot = false;
};

template<class Iterator>
class grammar final : public qi::grammar<Iterator, ast::node::program(), comment_skipper<Iterator>> {
    template<class Value, class... Extra>
    using rule = qi::rule<Iterator, Value, comment_skipper<Iterator>, Extra...>;

public:
    grammar(Iterator const code_begin) noexcept
        : grammar::base_type(program)
    {

        sep = +(lit(';') ^ qi::eol);

        stmt_block_before_end
            = (
                -((compound_stmt - "end") % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        program
            = (
                -sep > -(global_definition % sep) > -sep > (qi::eol | qi::eoi)
            ) [
                _val = make_node_ptr<ast::node::program>(as_vector(_1))
            ];

        literal
            = (
                  character_literal
                | string_literal
                | boolean_literal
                | float_literal
                | integer_literal
                | array_literal
                | symbol_literal
                | dict_literal
                | tuple_literal
            ) [
                _val = make_node_ptr<ast::node::literal>(_1)
            ];

        character_literal
            = (
                '\''
                > ((qi::char_ - ascii::cntrl - '\\' - '\'')
                | ('\\' > (
                          qi::char_('b')[_1 = '\b']
                        | qi::char_('f')[_1 = '\f']
                        | qi::char_('n')[_1 = '\n']
                        | qi::char_('r')[_1 = '\r']
                        | qi::char_('\\')[_1 = '\\']
                        | qi::char_('\'')[_1 = '\'']
                    ))
                ) > '\''
            ) [
                _val = make_node_ptr<ast::node::character_literal>(_1)
            ];

        float_literal
            = (
                (qi::real_parser<double, strict_real_policies_disallowing_trailing_dot<double>>())
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
                qi::as_string[qi::lexeme['"' > *(
                    (qi::char_ - '"' - '\\' - ascii::cntrl) |
                    ('\\' >> (
                          qi::char_('b')[_1 = '\b']
                        | qi::char_('f')[_1 = '\f']
                        | qi::char_('n')[_1 = '\n']
                        | qi::char_('r')[_1 = '\r']
                        | qi::char_('\\')[_1 = '\\']
                        | qi::char_('"')[_1 = '"']
                        | (qi::char_ - ascii::cntrl)
                    ))
                ) > '"']]
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
                    compound_expr % ','
                ) >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_literal>(as_vector(_1))
            ];

        tuple_literal
            = (
                '(' >> -(
                    compound_expr[phx::push_back(_a, _1)]
                    >> +(
                        ',' >> compound_expr[phx::push_back(_a, _1)]
                    )
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_literal>(_a)
            ];

        symbol_literal
            = qi::lexeme[
                ':' >>
                qi::as_string[
                    +(qi::alnum | qi::char_("=*/%+><&^|&!~_-"))
                ][
                    _val = make_node_ptr<ast::node::symbol_literal>(_1)
                ]
            ];

        dict_literal
            = (
                '{' >>
                    -(
                        qi::as<ast::node_type::dict_literal::dict_elem_type>()[
                            compound_expr > "=>" > compound_expr
                        ] % ','
                    ) >> -(lit(',')) // allow trailing comma
                >> '}'
            ) [
                _val = make_node_ptr<ast::node::dict_literal>(as_vector(_1))
            ];

        function_name
            = (
                qi::as_string[
                    qi::lexeme[
                        (qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_')) >> -qi::char_("?!'")
                    ]
                ]
            );

        variable_name
            = (
                qi::as_string[
                    qi::lexeme[
                        (qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_'))
                    ]
                ]
            );

        type_name = variable_name.alias();

        var_ref
            = (
                // Note:
                // This is because var_ref matches an identifier on function call.
                // function name means an immutable function variable and its name
                // should subject to a rule of function name.  If var_ref doesn't
                // represent a function, it should not subject to a rule of function
                // name.  In the case, semantic check should reject the name.
                function_name
            ) [
                _val = make_node_ptr<ast::node::var_ref>(_1)
            ];

        parameter
            = (
                -qi::string("var")
                >> variable_name
                >> -(
                    ':' >> qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::parameter>(_1, _2, _3)
            ];

        function_call
            = (
                '(' >> -(
                    compound_expr % ','
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::function_call>(as_vector(_1))
            ];

        constructor_call
            = (
                '{' >> -(
                    compound_expr[phx::push_back(_val, _1)] % ','
                ) >> '}'
            );

        object_construct
            = (
                    qualified_type >> constructor_call
            ) [
                _val = make_node_ptr<ast::node::object_construct>(_1, _2)
            ];

        primary_expr
            = (
                object_construct
                | literal
                | var_ref
                | '(' >> compound_expr >> ')'
            ) [
                _val = make_node_ptr<ast::node::primary_expr>(_1)
            ];

        index_access
            = (
                '[' >> compound_expr >> ']'
            ) [
                _val = make_node_ptr<ast::node::index_access>(_1)
            ];

        member_access
            = (
                '.' >> function_name
            ) [
                _val = make_node_ptr<ast::node::member_access>(_1)
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

        primary_type
            = (
                type_name >> -(
                    '(' >> (qualified_type % ',') >> ')'
                )
            ) [
                _val = make_node_ptr<ast::node::primary_type>(_1, _2)
            ];

        nested_type
            = (
                ('(' >> qualified_type >> ')') | primary_type
            ) [
                _val = make_node_ptr<ast::node::nested_type>(_1)
            ];

        array_type
            = (
                '[' >> qualified_type >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_type>(_1)
            ];

        dict_type
            = (
                '{' >> qualified_type >> "=>" >> qualified_type >> '}'
            ) [
                _val = make_node_ptr<ast::node::dict_type>(_1, _2)
            ];

        tuple_type
            = (
                '('
                >> -(
                    qualified_type[phx::push_back(_a, _1)] >> +(',' >> qualified_type[phx::push_back(_a, _1)])
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_type>(_a)
            ];

        func_type
            = (
                lit("func") >> '(' >> -(qualified_type % ',') >> ')' >> ':' >> qualified_type
            ) [
                _val = make_node_ptr<ast::node::func_type>(as_vector(_1), _2)
            ];

        proc_type
            = (
                lit("proc") >> '(' >> -(qualified_type % ',') >> ')'
            ) [
                _val = make_node_ptr<ast::node::proc_type>(as_vector(_1))
            ];

        compound_type
            = (
                func_type | proc_type | array_type | dict_type | tuple_type | nested_type
            ) [
                _val = make_node_ptr<ast::node::compound_type>(_1)
            ];

        qualified_type
            = (
                compound_type >> -qualifier
            ) [
                _val = make_node_ptr<ast::node::qualified_type>(_2, _1)
            ];

        cast_expr
            = (
                unary_expr >> *("as" > qualified_type)
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

        range_expr
            = (
                logical_or_expr >> -(
                    qi::as<ast::node_type::range_expr::rhs_type>()[
                        range_kind >> logical_or_expr
                    ]
                )
            ) [
                _val = make_node_ptr<ast::node::range_expr>(_1, _2)
            ];

        if_expr
            = (
                if_kind >> (compound_expr - "then") >> ("then" || sep)
                >> (compound_expr - "else") >> -sep >> "else" >> -sep >> compound_expr
            ) [
                _val = make_node_ptr<ast::node::if_expr>(_1, _2, _3, _4)
            ];

        compound_expr
            = (
                (if_expr | range_expr) >> -(':' >> qualified_type)
            ) [
                _val = make_node_ptr<ast::node::compound_expr>(_1, _2)
            ];

        assignment_stmt
            = (
                postfix_expr % ',' >> assign_operator >> compound_expr % ','
            ) [
                _val = make_node_ptr<ast::node::assignment_stmt>(_1, _2, _3)
            ];

        variable_decl
            = (
                -(qi::string("var")) >> variable_name >> -(':' >> qualified_type)
            ) [
                _val = make_node_ptr<ast::node::variable_decl>(_1, _2, _3)
            ];

        initialize_stmt
            = (
                variable_decl % ',' >> ":=" >> compound_expr % ','
            ) [
                _val = make_node_ptr<ast::node::initialize_stmt>(_1, _2)
            ];

        if_then_stmt_block
            = (
                -((compound_stmt - "end" - "elseif" - "else" - "then") % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        if_else_stmt_block
            = (
                -((compound_stmt - "end") % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        if_stmt
            = (
                if_kind >> (compound_expr - "then") >> ("then" || sep)
                >> if_then_stmt_block >> -sep
                >> *(
                    qi::as<ast::node_type::if_stmt::elseif_type>()[
                        "elseif" >> (compound_expr - "then") >> ("then" || sep)
                        >> if_then_stmt_block >> -sep
                    ]
                ) >> -("else" >> -sep >> if_else_stmt_block >> -sep)
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::if_stmt>(_1, _2, _3, _4, _5)
            ];

        return_stmt
            = (
                "return" >> -(compound_expr % ',')
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(as_vector(_1))
            ];

        case_when_stmt_block
            = (
                *((compound_stmt - "end" - "else") >> sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        case_stmt
            = (
                "case" >> sep
                >> +(
                    qi::as<ast::node_type::case_stmt::when_type>()[
                        "when" >> (compound_expr - "then") >> ("then" || sep)
                        >> case_when_stmt_block
                    ]
                ) >> -(
                    "else" >> -sep >> stmt_block_before_end >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::case_stmt>(_1, _2)
            ];

        switch_stmt
            = (
                "case" >> (compound_expr - "when") >> sep
                >> +(
                    qi::as<ast::node_type::switch_stmt::when_type>()[
                        "when" >> (compound_expr - "then") % ',' >> ("then" || sep)
                        >> case_when_stmt_block
                    ]
                ) >> -(
                    "else" >> -sep >> stmt_block_before_end >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::switch_stmt>(_1, _2, _3)
            ];

        for_stmt
            = (
                // Note: "do" might colide with do-end block in compound_expr
                "for" >> (parameter - "in") % ',' >> "in" >> compound_expr >> ("do" || sep)
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::for_stmt>(_1, _2, _3)
            ];

        while_stmt
            = (
                // Note: "do" might colide with do-end block in compound_expr
                "for" >> compound_expr >> ("do" || sep)
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::while_stmt>(_1, _2)
            ];

        postfix_if_return_stmt
            = (
                "return" >> -((compound_expr - if_kind) % ',')
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(as_vector(_1))
            ];

        postfix_if_stmt
            = (
                (
                    postfix_if_return_stmt
                  | assignment_stmt
                  | (compound_expr - if_kind)
                )
                >> if_kind >> compound_expr
            ) [
                _val = make_node_ptr<ast::node::postfix_if_stmt>(_1, _2, _3)
            ];

        compound_stmt
            = (
                  if_stmt
                | case_stmt
                | switch_stmt
                | for_stmt
                | while_stmt
                | initialize_stmt
                | postfix_if_stmt
                | return_stmt
                | assignment_stmt
                | compound_expr
            ) [
                _val = make_node_ptr<ast::node::compound_stmt>(_1)
            ];

        function_param_decls
            = -('(' >> -((parameter % ',')[_val = _1]) > ')');

        // FIXME: Temporary
        // Spec of precondition is not determined yet...
        func_precondition
            = -("begin" > sep);

        func_body_stmt_block
            = (
                -((compound_stmt - "ensure" - "end") % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        function_definition
            = (
                "func" > function_name > function_param_decls > -(':' > qualified_type) > sep
                > func_precondition
                > func_body_stmt_block > -sep
                > -(
                    "ensure" > sep > stmt_block_before_end > -sep
                ) > "end"
            )[
                _val = make_node_ptr<ast::node::function_definition>(_1, _2, _3, _4, _5)
            ];

        procedure_definition
            = (
                "proc" > function_name > function_param_decls > sep
                > func_precondition
                > func_body_stmt_block > -sep
                > -(
                    "ensure" > sep > stmt_block_before_end > -sep
                ) > "end"
            )[
                _val = make_node_ptr<ast::node::procedure_definition>(_1, _2, _3, _4)
            ];

        constant_decl
            = (
                variable_name >> -(':' >> qualified_type)
            ) [
                _val = make_node_ptr<ast::node::constant_decl>(_1, _2)
            ];

        constant_definition
            = (
                constant_decl % ',' >> ":=" >> compound_expr % ','
            ) [
                _val = make_node_ptr<ast::node::constant_definition>(_1, _2)
            ];

        global_definition
            = (
                function_definition |
                procedure_definition |
                constant_definition
            ) [
                _val = make_node_ptr<ast::node::global_definition>(_1)
            ];

        // Set callback to get the position of node and show obvious compile error {{{
        detail::set_position_getter_on_success(

            // _val : parsed value
            // _1   : position before parsing
            // _2   : end of string to parse
            // _3   : position after parsing
            phx::bind(
                [code_begin](auto &node_ptr, auto const before, auto const after)
                {
                    node_ptr->line = spirit::get_line(before);
                    node_ptr->col = spirit::get_column(code_begin, before);
                    node_ptr->length = std::distance(before.base(), after.base());
                }
                , _val, _1, _3)

            , program
            , literal
            , integer_literal
            , character_literal
            , float_literal
            , boolean_literal
            , string_literal
            , array_literal
            , tuple_literal
            , symbol_literal
            , dict_literal
            , var_ref
            , parameter
            , function_call
            , object_construct
            , primary_expr
            , index_access
            , member_access
            , postfix_expr
            , unary_expr
            , primary_type
            , nested_type
            , array_type
            , dict_type
            , tuple_type
            , func_type
            , proc_type
            , compound_type
            , qualified_type
            , cast_expr
            , mult_expr
            , additive_expr
            , shift_expr
            , relational_expr
            , equality_expr
            , and_expr
            , xor_expr
            , or_expr
            , logical_and_expr
            , logical_or_expr
            , range_expr
            , if_expr
            , compound_expr
            , if_stmt
            , return_stmt
            , case_stmt
            , switch_stmt
            , for_stmt
            , while_stmt
            , variable_decl
            , initialize_stmt
            , assignment_stmt
            , postfix_if_return_stmt
            , postfix_if_stmt
            , compound_stmt
            , function_definition
            , procedure_definition
            , constant_decl
            , constant_definition
            , global_definition
            , stmt_block_before_end
            , if_then_stmt_block
            , if_else_stmt_block
            , case_when_stmt_block
            , func_body_stmt_block
        );

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
                      << "expected " << _4
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
        // }}}

        // Rule names {{{
        program.name("program");
        literal.name("literal");
        integer_literal.name("integer literal");
        character_literal.name("character literal");
        float_literal.name("float literal");
        boolean_literal.name("boolean literal");
        string_literal.name("string literal");
        array_literal.name("array literal");
        tuple_literal.name("tuple literal");
        symbol_literal.name("symbol literal");
        dict_literal.name("dictionary literal");
        var_ref.name("variable reference");
        parameter.name("parameter");
        function_call.name("function call");
        object_construct.name("object contruction");
        primary_expr.name("primary expression");
        index_access.name("index access");
        member_access.name("member access");
        postfix_expr.name("postfix expression");
        unary_expr.name("unary expression");
        primary_type.name("template type");
        nested_type.name("primary type");
        array_type.name("array type");
        dict_type.name("dictionary type");
        tuple_type.name("tuple type");
        func_type.name("function type");
        proc_type.name("procedure type");
        compound_type.name("compound type");
        qualified_type.name("qualified type");
        cast_expr.name("cast expression");
        mult_expr.name("multiply expression");
        additive_expr.name("additive expression");
        shift_expr.name("shift expression");
        relational_expr.name("relational expression");
        equality_expr.name("equality expression");
        and_expr.name("and expression");
        xor_expr.name("exclusive or expression");
        or_expr.name("or expression");
        logical_and_expr.name("logical and expression");
        logical_or_expr.name("logical or expression");
        range_expr.name("range expression");
        if_expr.name("if expression");
        compound_expr.name("compound expression");
        if_stmt.name("if statement");
        return_stmt.name("return statement");
        case_stmt.name("case statement");
        switch_stmt.name("switch statement");
        for_stmt.name("for statement");
        while_stmt.name("while statement");
        variable_decl.name("variable declaration");
        initialize_stmt.name("initialize statement");
        assignment_stmt.name("assignment statement");
        postfix_if_stmt.name("prefix if statement");
        compound_stmt.name("compound statement");
        function_definition.name("function definition");
        procedure_definition.name("procedure definition");
        constant_decl.name("constant declaration");
        constant_definition.name("constant definition");
        global_definition.name("global definition");
        sep.name("separater");
        constructor_call.name("constructor call");
        function_param_decls.name("parameter declarations of function");
        stmt_block_before_end.name("statements block before 'end'");
        if_then_stmt_block.name("'then' clause in if statement");
        if_else_stmt_block.name("'else' clause in if statement");
        case_when_stmt_block.name("'when' clause in case statement");
        func_body_stmt_block.name("statements in body of function");
        function_name.name("function name");
        variable_name.name("variable name");
        type_name.name("type name");
        func_precondition.name("precondition in function");
        postfix_if_return_stmt.name("return statement in postfix if statement");
        // }}}
    }

    ~grammar()
    {}

private:

    // Rules {{{
    rule<qi::unused_type()> sep;

#define DACHS_DEFINE_RULE(n) rule<ast::node::n()> n
#define DACHS_DEFINE_RULE_WITH_LOCALS(n, ...) rule<ast::node::n(), qi::locals< __VA_ARGS__ >> n

    DACHS_DEFINE_RULE(program);
    DACHS_DEFINE_RULE(literal);
    DACHS_DEFINE_RULE(integer_literal);
    DACHS_DEFINE_RULE(character_literal);
    DACHS_DEFINE_RULE(float_literal);
    DACHS_DEFINE_RULE(boolean_literal);
    DACHS_DEFINE_RULE(string_literal);
    DACHS_DEFINE_RULE(array_literal);
    DACHS_DEFINE_RULE_WITH_LOCALS(tuple_literal, std::vector<ast::node::compound_expr>);
    DACHS_DEFINE_RULE(symbol_literal);
    DACHS_DEFINE_RULE(dict_literal);
    DACHS_DEFINE_RULE(var_ref);
    DACHS_DEFINE_RULE(parameter);
    DACHS_DEFINE_RULE(function_call);
    DACHS_DEFINE_RULE(object_construct);
    DACHS_DEFINE_RULE(primary_expr);
    DACHS_DEFINE_RULE(index_access);
    DACHS_DEFINE_RULE(member_access);
    DACHS_DEFINE_RULE(postfix_expr);
    DACHS_DEFINE_RULE(unary_expr);
    DACHS_DEFINE_RULE(primary_type);
    DACHS_DEFINE_RULE(nested_type);
    DACHS_DEFINE_RULE(array_type);
    DACHS_DEFINE_RULE(dict_type);
    DACHS_DEFINE_RULE_WITH_LOCALS(tuple_type, std::vector<ast::node::qualified_type>);
    DACHS_DEFINE_RULE(func_type);
    DACHS_DEFINE_RULE(proc_type);
    DACHS_DEFINE_RULE(compound_type);
    DACHS_DEFINE_RULE(qualified_type);
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
    DACHS_DEFINE_RULE(range_expr);
    DACHS_DEFINE_RULE(if_expr);
    DACHS_DEFINE_RULE(compound_expr);
    DACHS_DEFINE_RULE(if_stmt);
    DACHS_DEFINE_RULE(return_stmt);
    DACHS_DEFINE_RULE(case_stmt);
    DACHS_DEFINE_RULE(switch_stmt);
    DACHS_DEFINE_RULE(for_stmt);
    DACHS_DEFINE_RULE(while_stmt);
    DACHS_DEFINE_RULE(variable_decl);
    DACHS_DEFINE_RULE(initialize_stmt);
    DACHS_DEFINE_RULE(assignment_stmt);
    DACHS_DEFINE_RULE(postfix_if_stmt);
    DACHS_DEFINE_RULE(compound_stmt);
    DACHS_DEFINE_RULE(function_definition);
    DACHS_DEFINE_RULE(procedure_definition);
    DACHS_DEFINE_RULE(constant_decl);
    DACHS_DEFINE_RULE(constant_definition);
    DACHS_DEFINE_RULE(global_definition);

    rule<std::vector<ast::node::compound_expr>()> constructor_call;
    rule<std::vector<ast::node::parameter>()> function_param_decls;
    rule<ast::node::statement_block()> stmt_block_before_end
                                     , if_then_stmt_block
                                     , if_else_stmt_block
                                     , case_when_stmt_block
                                     , func_body_stmt_block
                                     ;
    rule<std::string()> function_name, variable_name, type_name;
    rule<qi::unused_type()/*TMP*/> func_precondition;
    decltype(return_stmt) postfix_if_return_stmt;

#undef DACHS_DEFINE_RULE
#undef DACHS_DEFINE_RULE_WITH_LOCALS
    // }}}

    // Symbol tables {{{
    struct unary_operator_rule_type : public qi::symbols<char, ast::symbol::unary_operator> {
        unary_operator_rule_type()
        {
            add
                ("+", ast::symbol::unary_operator::positive)
                ("-", ast::symbol::unary_operator::negative)
                ("~", ast::symbol::unary_operator::one_complement)
                ("!", ast::symbol::unary_operator::logical_negate)
            ;
        }
    } unary_operator;

    struct mult_operator_rule_type : public qi::symbols<char, ast::symbol::mult_operator> {
        mult_operator_rule_type()
        {
            add
                ("*", ast::symbol::mult_operator::mult)
                ("/", ast::symbol::mult_operator::div)
                ("%", ast::symbol::mult_operator::mod)
            ;
        }
    } mult_operator;

    struct additive_operator_rule_type : public qi::symbols<char, ast::symbol::additive_operator> {
        additive_operator_rule_type()
        {
            add
                ("+", ast::symbol::additive_operator::add)
                ("-", ast::symbol::additive_operator::sub)
            ;
        }
    } additive_operator;

    struct relational_operator_rule_type : public qi::symbols<char, ast::symbol::relational_operator> {
        relational_operator_rule_type()
        {
            add
                ("<", ast::symbol::relational_operator::less_than)
                (">", ast::symbol::relational_operator::greater_than)
                ("<=", ast::symbol::relational_operator::less_than_equal)
                (">=", ast::symbol::relational_operator::greater_than_equal)
            ;
        }
    } relational_operator;

    struct shift_operator_rule_type : public qi::symbols<char, ast::symbol::shift_operator> {
        shift_operator_rule_type()
        {
            add
                (">>", ast::symbol::shift_operator::left)
                ("<<", ast::symbol::shift_operator::right)
            ;
        }
    } shift_operator;

    struct equality_operator_rule_type : public qi::symbols<char, ast::symbol::equality_operator> {
        equality_operator_rule_type()
        {
            add
                ("==", ast::symbol::equality_operator::equal)
                ("!=", ast::symbol::equality_operator::not_equal)
            ;
        }
    } equality_operator;

    struct assign_operator_rule_type : public qi::symbols<char, ast::symbol::assign_operator> {
        assign_operator_rule_type()
        {
            add
                ("=", ast::symbol::assign_operator::assign)
                ("*=", ast::symbol::assign_operator::mult)
                ("/=", ast::symbol::assign_operator::div)
                ("%=", ast::symbol::assign_operator::mod)
                ("+=", ast::symbol::assign_operator::add)
                ("-=", ast::symbol::assign_operator::sub)
                ("<<=", ast::symbol::assign_operator::left_shift)
                (">>=", ast::symbol::assign_operator::right_shift)
                ("&=", ast::symbol::assign_operator::arithmetic_and)
                ("^=", ast::symbol::assign_operator::arithmetic_xor)
                ("|=", ast::symbol::assign_operator::arithmetic_or)
                ("&&=", ast::symbol::assign_operator::logical_and)
                ("||=", ast::symbol::assign_operator::logical_or)
            ;
        }
    } assign_operator;

    struct if_kind_rule_type : public qi::symbols<char, ast::symbol::if_kind> {
        if_kind_rule_type()
        {
            add
                ("if", ast::symbol::if_kind::if_)
                ("unless", ast::symbol::if_kind::unless)
            ;
        }
    } if_kind;

    struct qualifier_rule_type : public qi::symbols<char, ast::symbol::qualifier> {
        qualifier_rule_type()
        {
            add
                ("?", ast::symbol::qualifier::maybe)
            ;
        }
    } qualifier;

    struct range_kind_rule_type : public qi::symbols<char, ast::symbol::range_kind> {
        range_kind_rule_type()
        {
            add
                ("...", ast::symbol::range_kind::exclusive)
                ("..", ast::symbol::range_kind::inclusive)
            ;
        }
    } range_kind;
    // }}}
};

ast::ast parser::parse(std::string const& code)
{
    auto itr = detail::line_pos_iterator(std::begin(code));
    using iterator_type = decltype(itr);
    auto const begin = itr;
    auto const end = detail::line_pos_iterator(std::end(code));
    grammar<iterator_type> spiritual_parser(begin);
    comment_skipper<iterator_type> skipper;
    ast::node::program root;

    if (!qi::phrase_parse(itr, end, spiritual_parser, skipper, root) || itr != end) {
        throw parse_error{spirit::get_line(itr), spirit::get_column(begin, itr)};
    }

    return {root};
}

} // namespace syntax
} // namespace dachs
