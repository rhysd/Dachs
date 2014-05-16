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
#include "dachs/helper/variant.hpp"
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
using qi::_6;
using qi::_a;
using qi::_val;
using qi::lit;
using qi::string;
// }}}

// Helpers {{{
namespace detail {

    using helper::variant::apply_lambda;

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

    template<class CodeIter>
    struct position_getter {
        CodeIter const code_begin;

        explicit position_getter(CodeIter const begin) noexcept
            : code_begin{begin}
        {}

        template<class T, class Iter>
        void operator()(std::shared_ptr<T> const& node_ptr, Iter const before, Iter const after) const noexcept
        {
            auto const d = std::distance(before.base(), after.base());
            node_ptr->line = spirit::get_line(before);
            node_ptr->col = spirit::get_column(code_begin, before);
            node_ptr->length = d < 0 ? 0 : d;
        }

        template<class Iter, class... Args>
        void operator()(boost::variant<Args...> const& node_variant, Iter const before, Iter const after) const noexcept
        {
            apply_lambda(
                [before, after, this](auto const& node)
                {
                    auto const d = std::distance(before.base(), after.base());
                    node->line = spirit::get_line(before);
                    node->col = spirit::get_column(code_begin, before);
                    node->length = d < 0 ? 0 : d;
                }, node_variant);
        }
    };
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

        sep = +(';' ^ qi::eol);

        comma = (',' >> -qi::eol) | (-qi::eol >> ',');

        trailing_comma = -(',' || qi::eol);

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

        character_literal
            = qi::lexeme[
                '\''
                > ((qi::char_ - ascii::cntrl - '\\' - '\'')[_val = _1]
                | ('\\' > (
                          qi::char_('b')[_val = '\b']
                        | qi::char_('f')[_val = '\f']
                        | qi::char_('n')[_val = '\n']
                        | qi::char_('r')[_val = '\r']
                        | qi::char_('t')[_val = '\t']
                        | qi::char_('\\')[_val = '\\']
                        | qi::char_('\'')[_val = '\'']
                    ))
                ) > '\''
            ];

        float_literal
            = (
                (qi::real_parser<double, strict_real_policies_disallowing_trailing_dot<double>>())
            );

        boolean_literal
            = (
                qi::bool_
            );

        string_literal
            = (
                qi::lexeme['"' > *(
                    (qi::char_ - '"' - '\\' - ascii::cntrl)[_val += _1] |
                    ('\\' >> (
                          qi::char_('b')[_val += '\b']
                        | qi::char_('f')[_val += '\f']
                        | qi::char_('n')[_val += '\n']
                        | qi::char_('r')[_val += '\r']
                        | qi::char_('t')[_val += '\t']
                        | qi::char_('\\')[_val += '\\']
                        | qi::char_('"')[_val += '"']
                        | (qi::char_ - ascii::cntrl)[_val += _1]
                    ))
                ) > '"']
            );

        integer_literal
            = (
                qi::int_
            );

        uinteger_literal
            = (
                qi::lexeme[qi::uint_ >> 'u']
            );

        array_literal
            = (
                '[' >> -(
                    -qi::eol >> typed_expr % comma >> trailing_comma
                ) >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_literal>(as_vector(_1))
            ];

        tuple_literal
            = (
                '(' >> -(
                    -qi::eol >> typed_expr[phx::push_back(_a, _1)]
                    >> +(
                        comma >> typed_expr[phx::push_back(_a, _1)]
                    ) >> trailing_comma
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_literal>(_a)
            ];

        symbol_literal
            = (
                qi::lexeme[
                ':' >>
                    +(qi::alnum | qi::char_("=*/%+><&^|&!~_-"))[_a += _1]
                ]
            ) [
                _val = make_node_ptr<ast::node::symbol_literal>(_a)
            ];

        dict_literal
            = (
                '{' >>
                    -(
                        (
                            -qi::eol >> (
                                qi::as<ast::node_type::dict_literal::dict_elem_type>()[
                                    typed_expr > "=>" > typed_expr
                                ] % comma
                            ) >> trailing_comma
                        )[_1] // Note: Avoid bug
                    )
                >> '}'
            ) [
                _val = make_node_ptr<ast::node::dict_literal>(as_vector(_1))
            ];

        primary_literal
            = (
                boolean_literal
              | character_literal
              | string_literal
              | float_literal
              | uinteger_literal
              | integer_literal
            ) [
                _val = make_node_ptr<ast::node::primary_literal>(_1)
            ];

        literal
            =
                  primary_literal
                | array_literal
                | symbol_literal
                | dict_literal
                | tuple_literal
            ;

        called_function_name
            =
                qi::lexeme[
                    (qi::alpha | qi::char_('_'))[_val += _1]
                    >> *(qi::alnum | qi::char_('_'))[_val += _1]
                    >> -qi::char_("?!'")[_val += _1]
                ]
            ;

        func_def_name
            =
                binary_operator | unary_operator | called_function_name
            ;

        variable_name
            = (
                qi::lexeme[
                    (qi::alpha | qi::char_('_'))[_val += _1]
                    >> *(qi::alnum | qi::char_('_'))[_val += _1]
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
                called_function_name
            ) [
                _val = make_node_ptr<ast::node::var_ref>(_1)
            ];

        parameter
            = (
                -qi::string("var")
                >> variable_name
                >> -(
                    -qi::eol >> ':' >> -qi::eol >> qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::parameter>(_1, _2, _3)
            ];

        constructor_call
            = (
                '{' >> -(
                    -qi::eol >> typed_expr[phx::push_back(_val, _1)] % comma >> -qi::eol
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
                | '(' >> -qi::eol >> typed_expr >> -qi::eol >> ')'
            );

        postfix_expr
            =
                primary_expr[_val = _1] >> *(
                      (-qi::eol >> '.' >> -qi::eol >> called_function_name)[_val = make_node_ptr<ast::node::member_access>(_val, _1)]
                    | ('[' >> -qi::eol >> typed_expr >> -qi::eol >> ']')[_val = make_node_ptr<ast::node::index_access>(_val, _1)]
                    | ('(' >> -qi::eol >> -(typed_expr % comma) >> trailing_comma >> ')')[_val = make_node_ptr<ast::node::func_invocation>(_val, as_vector(_1))]
                )
            ;

        unary_operator
            =
                string("+")
              | string("-")
              | string("~")
              | string("!")
            ;

        binary_operator
            =
                string("...")
              | string("..")
              | string(">>")
              | string("<<")
              | string("<=")
              | string(">=")
              | string("==")
              | string("!=")
              | string("&&")
              | string("||")
              | string("*")
              | string("/")
              | string("%")
              | string("+")
              | string("-")
              | string("<")
              | string(">")
              | string("&")
              | string("^")
              | string("|")
            ;

        unary_expr
            =
                (
                    (
                        unary_operator >> unary_expr
                    )[
                        _val = make_node_ptr<ast::node::unary_expr>(_1, _2)
                    ]
                ) | postfix_expr[_val = _1]
            ;

        cast_expr
            =
                unary_expr[_val = _1] >>
                *(
                    (-qi::eol >> "as") > -qi::eol > qualified_type
                )[
                    _val = make_node_ptr<ast::node::cast_expr>(_val, _1)
                ]
            ;

        mult_expr
            = (
                cast_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        string("*")
                      | string("/")
                      | string("%")
                    ) >> -qi::eol >> cast_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            );

        additive_expr
            = (
                mult_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        string("+")
                      | string("-")
                    ) >> -qi::eol >> mult_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            );

        shift_expr
            =
                additive_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        string("<<")
                      | string(">>")
                    ) >> -qi::eol >> additive_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            ;

        relational_expr
            =
                shift_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        string("<=")
                      | string(">=")
                      | string("<")
                      | string(">")
                    ) >> -qi::eol >> shift_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            ;

        equality_expr
            =
                relational_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        string("==")
                      | string("!=")
                    ) >> -qi::eol >> relational_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            ;

        and_expr
            =
                equality_expr[_val = _1] >>
                *(
                    -qi::eol >> "&" >> -qi::eol >> equality_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, "&", _1)
                ]
            ;

        xor_expr
            =
                and_expr[_val = _1] >>
                *(
                    -qi::eol >> "^" >> -qi::eol >> and_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, "^", _1)
                ]
            ;

        or_expr
            =
                xor_expr[_val = _1] >>
                *(
                    -qi::eol >> "|" >> -qi::eol >> xor_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, "|", _1)
                ]
            ;

        logical_and_expr
            =
                or_expr[_val = _1] >>
                *(
                    -qi::eol >> "&&" >> -qi::eol >> or_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, "&&", _1)
                ]
            ;

        logical_or_expr
            =
                logical_and_expr[_val = _1] >>
                *(
                    -qi::eol >> "||" >> -qi::eol >> logical_and_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, "||", _1)
                ]
            ;

        range_expr
            =
                logical_or_expr[_val = _1] >>
                -(
                    -qi::eol >> (
                        string("...")
                      | string("..")
                    ) >> -qi::eol >> logical_or_expr
                )[
                    _val = make_node_ptr<ast::node::binary_expr>(_val, _1, _2)
                ]
            ;

        if_expr
            = (
                if_kind >> (typed_expr - "then") >> ("then" || sep)
                >> (typed_expr - "else") >> -sep >> "else" >> -sep >> typed_expr
            ) [
                _val = make_node_ptr<ast::node::if_expr>(_1, _2, _3, _4)
            ];

        typed_expr
            =
                (if_expr | range_expr)[_val = _1]
                >> -(
                    -qi::eol >> ':' >> -qi::eol >> qualified_type
                )[
                    _val = make_node_ptr<ast::node::typed_expr>(_val, _1)
                ]
            ;

        primary_type
            = (
                type_name >> -(
                    '(' >> -qi::eol >> (qualified_type % comma) >> -qi::eol >> ')'
                )
            ) [
                _val = make_node_ptr<ast::node::primary_type>(_1, as_vector(_2))
            ];

        nested_type
            = (
                ('(' >> -qi::eol >> qualified_type >> -qi::eol >> ')') | primary_type
            );

        array_type
            = (
                '[' >> -qi::eol >> qualified_type >> -qi::eol >> ']'
            ) [
                _val = make_node_ptr<ast::node::array_type>(_1)
            ];

        dict_type
            = (
                '{' >> -qi::eol >> qualified_type >> -qi::eol >> "=>" >> -qi::eol >> qualified_type >> -qi::eol >> '}'
            ) [
                _val = make_node_ptr<ast::node::dict_type>(_1, _2)
            ];

        tuple_type
            = (
                '('
                >> -(
                    -qi::eol >> qualified_type[phx::push_back(_a, _1)] >>
                    +(comma >> qualified_type[phx::push_back(_a, _1)]) >> trailing_comma
                ) >> ')'
            ) [
                _val = make_node_ptr<ast::node::tuple_type>(_a)
            ];

        func_type
            = (
                lit("func") >> '(' >> -qi::eol >> -(qualified_type % comma) >> trailing_comma >> ')' >>
                -qi::eol >> ':' >> -qi::eol >> qualified_type
            ) [
                _val = make_node_ptr<ast::node::func_type>(as_vector(_1), _2)
            ] | (
                lit("proc") >> '(' >> -qi::eol >> -(qualified_type % comma) >> trailing_comma >> ')'
            ) [
                _val = make_node_ptr<ast::node::func_type>(as_vector(_1))
            ];

        compound_type
            = (
                func_type | array_type | dict_type | tuple_type | nested_type
            );

        qualified_type
            =
                compound_type[_val = _1] >>
                -qualifier[_val = make_node_ptr<ast::node::qualified_type>(_1, _val)]
            ;

        assignment_stmt
            = (
                postfix_expr % comma >> assign_operator >> typed_expr % comma
            ) [
                _val = make_node_ptr<ast::node::assignment_stmt>(_1, _2, _3)
            ];

        variable_decl
            = (
                -(qi::string("var")) >> variable_name >> -(
                    // Note: In this paren, > can't be used because of :=
                    -qi::eol >> ':' >> -qi::eol >> qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::variable_decl>(_1, _2, _3)
            ];

        initialize_stmt
            = (
                variable_decl % comma >> trailing_comma
                >> ":=" >>
                -qi::eol >> typed_expr % comma /* Note: Disallow trailing comma in here because unexpected line continuation suffers */
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
                if_kind >> (typed_expr - "then") >> ("then" || sep)
                >> if_then_stmt_block >> -sep
                >> *(
                    qi::as<ast::node_type::if_stmt::elseif_type>()[
                        "elseif" >> (typed_expr - "then") >> ("then" || sep)
                        >> if_then_stmt_block >> -sep
                    ]
                ) >> -("else" >> -sep >> if_else_stmt_block >> -sep)
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::if_stmt>(_1, _2, _3, _4, _5)
            ];

        return_stmt
            = (
                "return" >> -(typed_expr % comma)
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
                        "when" >> (typed_expr - "then") >> ("then" || sep)
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
                "case" >> (typed_expr - "when") >> sep
                >> +(
                    qi::as<ast::node_type::switch_stmt::when_type>()[
                        "when" >> (typed_expr - "then") % comma >> ("then" || sep)
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
                // Note: "do" might colide with do-end block in typed_expr
                "for" >> (parameter - "in") % comma >> "in" >> typed_expr >> ("do" || sep)
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::for_stmt>(_1, _2, _3)
            ];

        while_stmt
            = (
                // Note: "do" might colide with do-end block in typed_expr
                "for" >> typed_expr >> ("do" || sep)
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::while_stmt>(_1, _2)
            ];

        postfix_if_return_stmt
            = (
                "return" >> -((typed_expr - if_kind) % comma)
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(as_vector(_1))
            ];

        postfix_if_stmt
            = (
                (
                    postfix_if_return_stmt
                  | assignment_stmt
                  | (typed_expr - if_kind)
                )
                >> if_kind >> typed_expr
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
                | typed_expr
            );

        function_param_decls
            = -(
                '(' >> -(
                    -qi::eol >> (parameter % comma)[_val = _1] >> trailing_comma
                ) > ')'
            );

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
                func_kind > func_def_name > function_param_decls > -((-qi::eol >> ':' >> -qi::eol) > qualified_type) > sep
                > func_precondition
                > func_body_stmt_block > -sep
                > -(
                    "ensure" > sep > stmt_block_before_end > -sep
                ) > "end"
            )[
                _val = make_node_ptr<ast::node::function_definition>(_1, _2, _3, _4, _5, _6)
            ];

        constant_decl
            = (
                variable_name >> -(':' >> -qi::eol >> qualified_type)
            ) [
                _val = make_node_ptr<ast::node::constant_decl>(_1, _2)
            ];

        constant_definition
            = (
                constant_decl % comma >> trailing_comma >> ":=" >> -qi::eol >> typed_expr % comma
            ) [
                _val = make_node_ptr<ast::node::constant_definition>(_1, _2)
            ];

        global_definition
            = (
                function_definition | constant_definition
            );

        // Set callback to get the position of node and show obvious compile error {{{
        detail::set_position_getter_on_success(

            // _val : parsed value
            // _1   : position before parsing
            // _2   : end of string to parse
            // _3   : position after parsing
            phx::bind(
                detail::position_getter<Iterator>{code_begin}
                , _val, _1, _3)

            , program
            , primary_literal
            , array_literal
            , tuple_literal
            , symbol_literal
            , dict_literal
            , var_ref
            , parameter
            , object_construct
            , postfix_expr
            , unary_expr
            , primary_type
            , array_type
            , dict_type
            , tuple_type
            , func_type
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
            , typed_expr
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
            , function_definition
            , constant_decl
            , constant_definition
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
        primary_literal.name("primary literal");
        integer_literal.name("integer literal");
        uinteger_literal.name("unsigned integer literal");
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
        object_construct.name("object contruction");
        unary_operator.name("unary operator");
        binary_operator.name("binary operator");
        primary_expr.name("primary expression");
        postfix_expr.name("postfix expression");
        unary_expr.name("unary expression");
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
        typed_expr.name("compound expression");
        primary_type.name("template type");
        nested_type.name("nested type");
        array_type.name("array type");
        dict_type.name("dictionary type");
        tuple_type.name("tuple type");
        func_type.name("function type");
        compound_type.name("compound type");
        qualified_type.name("qualified type");
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
        called_function_name.name("name of called function");
        func_def_name.name("name of function definition");
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
    rule<qi::unused_type()> sep, comma, trailing_comma;

#define DACHS_DEFINE_RULE(n) rule<ast::node::n()> n
#define DACHS_DEFINE_RULE_WITH_LOCALS(n, ...) rule<ast::node::n(), qi::locals< __VA_ARGS__ >> n

    DACHS_DEFINE_RULE(program);
    DACHS_DEFINE_RULE(parameter);
    DACHS_DEFINE_RULE(object_construct);
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
    DACHS_DEFINE_RULE(constant_decl);
    DACHS_DEFINE_RULE(constant_definition);
    DACHS_DEFINE_RULE(global_definition);

    rule<char()> character_literal;
    rule<double()> float_literal;
    rule<int()> integer_literal;
    rule<unsigned int()> uinteger_literal;
    rule<bool()> boolean_literal;
    rule<std::string()> string_literal;

    rule<ast::node::any_expr()>
          literal
        , primary_literal
        , array_literal
        , dict_literal
        , var_ref
        , primary_expr
        , postfix_expr
        , unary_expr
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
        , typed_expr
    ;

    rule<ast::node::any_expr(), qi::locals<std::vector<ast::node::any_expr>>> tuple_literal;
    rule<ast::node::any_expr(), qi::locals<std::string>> symbol_literal;

    rule<ast::node::any_type()>
          primary_type
        , nested_type
        , array_type
        , dict_type
        , func_type
        , compound_type
        , qualified_type
    ;

    rule<ast::node::any_type(), qi::locals<std::vector<ast::node::any_type>>> tuple_type;

    rule<std::vector<ast::node::any_expr>()> constructor_call;
    rule<std::vector<ast::node::parameter>()> function_param_decls;
    rule<ast::node::statement_block()> stmt_block_before_end
                                     , if_then_stmt_block
                                     , if_else_stmt_block
                                     , case_when_stmt_block
                                     , func_body_stmt_block
                                     ;
    rule<std::string()> called_function_name, func_def_name, variable_name, type_name, unary_operator, binary_operator;
    rule<qi::unused_type()/*TMP*/> func_precondition;
    decltype(return_stmt) postfix_if_return_stmt;

#undef DACHS_DEFINE_RULE
#undef DACHS_DEFINE_RULE_WITH_LOCALS
    // }}}

    // Symbol tables {{{
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

    struct func_kind_rule_type : public qi::symbols<char, ast::symbol::func_kind> {
        func_kind_rule_type()
        {
            add
                ("func", ast::symbol::func_kind::func)
                ("proc", ast::symbol::func_kind::proc)
            ;
        }
    } func_kind;
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
