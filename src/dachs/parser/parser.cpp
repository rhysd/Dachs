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
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/qi_as.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_object.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/spirit/include/phoenix_bind.hpp>
#include <boost/fusion/include/std_pair.hpp>

#include "dachs/parser/parser.hpp"
#include "dachs/parser/comment_skipper.hpp"
#include "dachs/ast/ast.hpp"
#include "dachs/exception.hpp"
#include "dachs/helper/variant.hpp"
#include "dachs/helper/colorizer.hpp"
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
using qi::_b;
using qi::_c;
using qi::_d;
using qi::_val;
using qi::alnum;
using qi::lexeme;
// }}}

// Helpers {{{
namespace detail {

    using helper::variant::apply_lambda;

    template<class Iterator>
    inline auto line_pos_iterator(Iterator i)
    {
        return boost::spirit::line_pos_iterator<Iterator>{i};
    }

    template<class Node>
    struct make_shared {
        bool do_not_allocate;

        template<class... Args>
        std::shared_ptr<Node> operator()(Args &&... args) const
        {
            if (do_not_allocate) {
                return nullptr;
            } else {
                return std::make_shared<Node>(std::forward<Args>(args)...);
            }
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
                    (*this)(node, before, after);
                }, node_variant);
        }
    };
} // namespace detail

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

// Literal parser literal
inline auto operator"" _l(char const* s, std::size_t)
{
    return qi::lit(s);
}

inline auto operator"" _l(char const c)
{
    return qi::lit(c);
}

// Parser literal
inline auto operator"" _p(char const c)
{
    return qi::char_(c);
}

// Parser literal
inline auto operator"" _p(char const* s, std::size_t)
{
    return qi::string(s);
}

// }}}

template<class FloatType>
struct strict_real_policies_disallowing_trailing_dot final
    : qi::strict_real_policies<FloatType> {
    static bool const allow_trailing_dot = false;
    // static bool const allow_leading_dot = false;
};

template<class Iterator>
class dachs_grammar final
    : public qi::grammar<
        Iterator,
        ast::node::inu(),
        comment_skipper<Iterator>,
        qi::locals<
            std::vector<ast::node::function_definition>,
            std::vector<ast::node::initialize_stmt>,
            std::vector<ast::node::class_definition>,
            std::vector<ast::node::import>
        >
    > {

    template<class Value, class... Extra>
    using rule = qi::rule<Iterator, Value, comment_skipper<Iterator>, Extra...>;

    helper::colorizer c;
    bool check_only;

    template<class NodeType, class... Holders>
    inline auto make_node_ptr(Holders &&... holders)
    {
        return phx::bind(detail::make_shared<typename NodeType::element_type>{check_only}, std::forward<Holders>(holders)...);
    }

public:
    dachs_grammar(Iterator const code_begin, bool const check_only) noexcept
        : dachs_grammar::base_type(inu), check_only(check_only)
    {

        // XXX:
        // Use macro instead of user-defined literal because rvalue of Boost.Spirit
        // parser must be copied by boost::proto::deep_copy. Or it causes SEGV.
        #define DACHS_KWD(...) ((__VA_ARGS__) >> !qi::no_skip[qi::alnum | '_'])

        sep = +(';' ^ qi::eol);

        comma = (',' >> -qi::eol) | (-qi::eol >> ',');

        trailing_comma = -(',' || qi::eol);

        stmt_block_before_end
            = (
                -((compound_stmt - DACHS_KWD("end")) % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        inu
            = (
                -sep > -(
                    (
                        function_definition[phx::push_back(_a, _1)]
                      | constant_definition[phx::push_back(_b, _1)]
                      | class_definition[phx::push_back(_c, _1)]
                      | import[phx::push_back(_d, _1)]
                    ) % sep
                ) > -sep > (qi::eol | qi::eoi)
            ) [
                _val = make_node_ptr<ast::node::inu>(_a, _b, _c, _d)
            ];

        character_literal
            = qi::lexeme[
                '\''
                > ((qi::char_ - ascii::cntrl - '\\' - '\'')[_val = _1]
                | ('\\' > (
                          'b'_p[_val = '\b']
                        | 'f'_p[_val = '\f']
                        | 'n'_p[_val = '\n']
                        | 'r'_p[_val = '\r']
                        | 't'_p[_val = '\t']
                        | '0'_p[_val += '\0']
                        | '\\'_p[_val = '\\']
                        | '\''_p[_val = '\'']
                    ))
                ) > '\''
            ];

        float_literal
            = (
                (qi::real_parser<double, strict_real_policies_disallowing_trailing_dot<double>>())
            );

        boolean_literal
            = (
                DACHS_KWD(qi::bool_)
            );

        string_literal
            = (
                qi::lexeme['"' > *(
                    (qi::char_ - '"' - '\\' - ascii::cntrl)[_val += _1] |
                    ('\\' >> (
                          'b'_p[_val += '\b']
                        | 'f'_p[_val += '\f']
                        | 'n'_p[_val += '\n']
                        | 'r'_p[_val += '\r']
                        | 't'_p[_val += '\t']
                        | '\\'_p[_val += '\\']
                        | '"'_p[_val += '"']
                        | (qi::char_ - ascii::cntrl)[_val += _1]
                    ))
                ) > '"']
            );

        integer_literal
            = qi::lexeme[
                ("0x" >> qi::hex)
              | ("0b" >> qi::bin)
              | ("0o" >> qi::oct)
              | qi::int_
            ];

        uinteger_literal
            = qi::lexeme[
                ((
                    ("0x" >> qi::hex)
                  | ("0b" >> qi::bin)
                  | ("0o" >> qi::oct)
                  | qi::uint_
                ) >> 'u') > !(qi::alnum | '_')
            ];

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
                    +(alnum | qi::char_("=*/%+><&^|&!~_-"))[_a += _1]
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
                                    typed_expr >> "=>" > typed_expr
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

        called_function_name
            =
                qi::lexeme[
                    -('@'_p[_val += _1])
                    >> (qi::alpha | '_'_p)[_val += _1]
                    >> *(alnum | '_'_p)[_val += _1]
                    >> -'?'_p[_val += _1]
                    >> *'\''_p[_val += _1]
                    >> -'!'_p[_val += _1]
                ]
            ;

        function_name
            =
                qi::lexeme[
                    (qi::alpha | '_'_p)[_val += _1]
                    >> *(alnum | '_'_p)[_val += _1]
                    >> -'?'_p[_val += _1]
                    >> *'\''_p[_val += _1]
                ]
            ;

        func_def_name
            =
                binary_operator | unary_operator | function_name
            ;

        variable_name
            = (
                qi::lexeme[
                    -('@'_p[_val += _1])
                    >> (qi::alpha | '_'_p)[_val += _1]
                    >> *(alnum | '_'_p)[_val += _1]
                    >> *'\''_p
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
                -DACHS_KWD(qi::matches["var"_l])
                >> variable_name // '@' reveals in constructors
                >> -(
                    -qi::eol >> ':' >> -qi::eol >> qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::parameter>(_1, _2, _3)
            ];

        constructor_call
            = (
                -('{' >> -(
                    -qi::eol >> typed_expr[phx::push_back(_val, _1)] % comma >> -qi::eol
                ) >> '}')
            );

        object_construct
            = (
                DACHS_KWD("new") >> qualified_type >> constructor_call >> -do_block
            ) [
                _val = make_node_ptr<ast::node::object_construct>(_1, _2, _3)
            ];

        // Note:
        // Use 2 temporary variables _a and _b. Causing back tracking on the parameters without parens
        // because of lack of "in", the body of lambda may be already parsed as parameter and it remains.
        // So, at first bind the parameters to _a and if "in" is parsed correctly, then substitute _a to _b.
        // Finally _b is used to construct ast::node::function_definition.
        lambda_expr_oneline
            = "->" >> -qi::eol >> (
                (
                   ('(' >> -(parameter[phx::push_back(_a, _1)] % comma >> trailing_comma) >> ')' >> -qi::eol >> DACHS_KWD("in")[_b = _a])
                | -(
                      (parameter - "in")[phx::push_back(_a, _1)] % comma >> trailing_comma >> DACHS_KWD("in")[_b = _a]
                   )
                ) >> -qi::eol >> typed_expr
            ) [
                _val = make_node_ptr<ast::node::function_definition>(
                    _b,
                    make_node_ptr<ast::node::statement_block>(
                        make_node_ptr<ast::node::return_stmt>(_2)
                    )
                )
            ];

        lambda_expr_do_end
            = "->" >> -qi::eol >> (
                (
                    ('(' >> -(parameter % comma >> trailing_comma) >> ')') | -((parameter - "do") % comma)
                ) >> -qi::eol >> DACHS_KWD("do") >> -qi::eol >> stmt_block_before_end >> -sep >> "end"
            ) [
                _val = make_node_ptr<ast::node::function_definition>(as_vector(_1), _2)
            ];

        lambda_expr
            =  (
                lambda_expr_do_end | lambda_expr_oneline
            ) [
                _val = make_node_ptr<ast::node::lambda_expr>(_1)
            ];

        begin_end_expr
            = (
                DACHS_KWD("begin") >> -qi::eol >> stmt_block_before_end >> -sep >> "end"
            ) [
                _val = phx::bind(
                        [check_only](auto const& statements)
                        {
                            if (check_only) {
                                return ast::node::func_invocation{};
                            }

                            return helper::make<ast::node::func_invocation>(
                                    helper::make<ast::node::lambda_expr>(
                                        helper::make<ast::node::function_definition>(
                                            std::vector<ast::node::parameter>{},
                                            statements
                                        )
                                    ),
                                    false, true // Note: Not let-in, but begin-end
                                );
                        }
                        , _1
                    )
            ];

        let_expr
            = (
                DACHS_KWD("let") >> -qi::eol
                >> (
                    initialize_stmt % sep
                ) [
                    _a = phx::bind(
                        [check_only](auto && inits)
                        {
                            if (check_only) {
                                return ast::node::statement_block{};
                            }

                            assert(!inits.empty());
                            auto stmts = helper::make<ast::node::statement_block>();
                            stmts->value.reserve(inits.size() + 1u);
                            stmts->set_source_location(*inits[0]);

                            for (auto && i : std::forward<decltype(inits)>(inits)) {
                                stmts->value.emplace_back(std::forward<decltype(i)>(i));
                            }

                            return stmts;
                        }, _1
                    )
                ] >> -qi::eol
                >> (
                    (
                        DACHS_KWD("in") >> -qi::eol >> typed_expr
                    ) [
                        phx::bind(
                            [check_only](auto const& body, auto const& expr)
                            {
                                if (check_only) {
                                    return;
                                }

                                auto ret = helper::make<ast::node::return_stmt>(expr);
                                ret->set_source_location(ast::node::location_of(expr));
                                body->value.emplace_back(
                                        std::move(ret)
                                    );
                            },
                            _a, _1
                        )
                    ] | (
                        DACHS_KWD("begin") >> -qi::eol >> stmt_block_before_end >> -sep >> "end"
                    ) [
                        phx::bind(
                            [check_only](auto const& body, auto && stmts)
                            {
                                if (check_only) {
                                    return;
                                }

                                std::move(std::begin(stmts->value), std::end(stmts->value), std::back_inserter(body->value));
                            },
                            _a, _1
                        )
                    ]
                )
            )[
                _val = phx::bind(
                        [check_only](auto && body)
                        {
                            if (check_only) {
                                return ast::node::func_invocation{};
                            }

                            return helper::make<ast::node::func_invocation>(
                                    helper::make<ast::node::lambda_expr>(
                                        helper::make<ast::node::function_definition>(
                                            std::vector<ast::node::parameter>{},
                                            std::forward<decltype(body)>(body)
                                        )
                                    ),
                                    false, true // Note: Not begin-end, but let-in
                                );
                        }, _a
                    )
            ];

        primary_expr
            = (
                  object_construct
                | lambda_expr
                | begin_end_expr
                | let_expr
                | primary_literal
                | array_literal
                | symbol_literal
                | dict_literal
                | tuple_literal
                | var_ref
                | '(' >> -qi::eol >> typed_expr >> -qi::eol >> ')'
            );

        do_block
            = (
                DACHS_KWD("do"_l) >> -(
                    '|' >> (parameter % comma) >> '|'
                ) >> -qi::eol >> stmt_block_before_end >> -sep >> "end"
            ) [
                _val = make_node_ptr<ast::node::function_definition>(as_vector(_1), _2)
            ] | (
                '{' >> -('|' >> (parameter % comma) >> '|') >> -qi::eol >> typed_expr >> -qi::eol >> '}'
            ) [
                _val = make_node_ptr<ast::node::function_definition>(
                        as_vector(_1),
                        make_node_ptr<ast::node::statement_block>(
                            make_node_ptr<ast::node::return_stmt>(_2)
                        )
                    )
            ];

        var_ref_before_space
            =
                var_ref >> qi::no_skip[&' '_l] >> !DACHS_KWD("as")
            ;

        // primary.name(...) [do-end]
        // primary.name ... do-end
        // primary.name ...
        // primary.name [do-end]
        // primary.name
        // primary ... do-end
        // primary[...]
        // primary(...)
        postfix_expr
            =
                // Note:
                // Third case doesn't permit using unary operator '+' and '-' in parameter because it is confusing if user intends to use binary operator '+' or '-'.
                // (e.g. (a.b + 10) should not be treated as a.b(+10))
                primary_expr[_val = _1] >> *(
                      (-qi::eol >> '.' >> -qi::eol >> var_ref >> '(' >> -(typed_expr % comma) >> trailing_comma >> ')' >> -do_block)[_val = make_node_ptr<ast::node::func_invocation>(_1, _val, as_vector(_2), _3)]
                    | (-qi::eol >> '.' >> -qi::eol >> var_ref_before_space >> (typed_expr - "do") % comma >> do_block)[_val = make_node_ptr<ast::node::func_invocation>(_1, _val, _2, _3)]
                    | (-qi::eol >> '.' >> -qi::eol >> var_ref_before_space >> !('+'_l | '-') >> (typed_expr - "do") % comma)[_val = make_node_ptr<ast::node::func_invocation>(_1, _val, _2)]
                    | (-qi::eol >> '.' >> -qi::eol >> (var_ref - "do") >> do_block)[_val = make_node_ptr<ast::node::func_invocation>(_2, _1, _val)]
                    | (-qi::eol >> '.' >> -qi::eol >> called_function_name)[_val = make_node_ptr<ast::node::ufcs_invocation>(_val, _1, ast::node_type::ufcs_invocation::set_location_tag{})]
                    | (qi::no_skip[&' '_l] >> !DACHS_KWD("as") >> (typed_expr - "do") % comma >> do_block)[_val = make_node_ptr<ast::node::func_invocation>(_val, _1, _2)]
                    | ('[' >> -qi::eol >> typed_expr >> -qi::eol >> ']')[_val = make_node_ptr<ast::node::index_access>(_val, _1)]
                    | ('(' >> -qi::eol >> -(typed_expr % comma) >> trailing_comma >> ')' >> -do_block)[_val = make_node_ptr<ast::node::func_invocation>(_val, as_vector(_1), _2)]
                )
            ;

        unary_operator
            =
                "+"_p
              | "-"_p
              | "~"_p
              | "!"_p
            ;

        binary_operator
            =
                "..."_p
              | ".."_p
              | ">>"_p
              | "<<"_p
              | "<="_p
              | ">="_p
              | "=="_p
              | "!="_p
              | "&&"_p
              | "||"_p
              | "*"_p
              | "/"_p
              | "%"_p
              | "+"_p
              | "-"_p
              | "<"_p
              | ">"_p
              | "&"_p
              | "^"_p
              | "|"_p
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
                    (-qi::eol >> DACHS_KWD("as")) > -qi::eol > qualified_type
                )[
                    _val = make_node_ptr<ast::node::cast_expr>(_val, _1)
                ]
            ;

        mult_expr
            = (
                cast_expr[_val = _1] >>
                *(
                    -qi::eol >> (
                        "*"_p
                      | "/"_p
                      | "%"_p
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
                        "+"_p
                      | "-"_p
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
                        "<<"_p
                      | ">>"_p
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
                        "<="_p
                      | ">="_p
                      | "<"_p
                      | ">"_p
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
                        "=="_p
                      | "!="_p
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
                        "..."_p
                      | ".."_p
                    ) >> -qi::eol >> logical_or_expr
                )[
                    _val = make_node_ptr<ast::node::object_construct>(_1, _val, _2)
                ]
            ;

        if_expr
            = (
                DACHS_KWD(if_kind) >> (typed_expr - DACHS_KWD("then")) >> (DACHS_KWD("then") || sep)
                >> (typed_expr - DACHS_KWD("else")) >> -sep >> DACHS_KWD("else") >> -sep >> typed_expr
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
                // Note: No need to use DACHS_KWD() because '(' or ':' follows it
                "func" >> -('(' >> -(-qi::eol >> qualified_type % comma >> trailing_comma) >> ')') >>
                -qi::eol >> ':' >> -qi::eol >> qualified_type
            ) [
                _val = make_node_ptr<ast::node::func_type>(as_vector(_1), _2)
            ] | (
                DACHS_KWD("proc") >> -('(' >> -(-qi::eol >> qualified_type % comma >> trailing_comma) >> ')')
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

        assign_operator
            =
                "="_p
              | "*="_p
              | "/="_p
              | "%="_p
              | "+="_p
              | "-="_p
              | "<<="_p
              | ">>="_p
              | "&="_p
              | "^="_p
              | "|="_p
              | "&&="_p
              | "||="_p
            ;

        assignment_stmt
            = (
                typed_expr % comma >> assign_operator >> typed_expr % comma
            ) [
                _val = make_node_ptr<ast::node::assignment_stmt>(_1, _2, _3)
            ];

        variable_decl
            = (
                -DACHS_KWD(qi::matches["var"_l])
                >> variable_name >> -(
                    // Note: In this paren, > can't be used because of :=
                    -qi::eol >> ':' >> -qi::eol >> qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::variable_decl>(_1, _2, _3)
            ];

        variable_decl_without_init
            = (
                DACHS_KWD("var"_l)
                >> variable_name >> -qi::eol >> ':' >> -qi::eol >> qualified_type
            ) [
                _val = make_node_ptr<ast::node::variable_decl>(true, _1, _2)
            ];


        initialize_stmt
            = (
                (
                    variable_decl % comma
                    >> trailing_comma >> ":="
                    >> -qi::eol >> typed_expr % comma
                    // Note:
                    // Disallow trailing comma in here because unexpected line continuation suffers
                ) [
                    _val = make_node_ptr<ast::node::initialize_stmt>(_1, _2)
                ]
            ) |
            (
                (
                    variable_decl_without_init % comma
                ) [
                    _val = make_node_ptr<ast::node::initialize_stmt>(_1)
                ]
            );

        if_then_stmt_block
            = (
                -((compound_stmt - DACHS_KWD("end") - DACHS_KWD("elseif") - DACHS_KWD("else") - DACHS_KWD("then")) % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        if_else_stmt_block
            = (
                -((compound_stmt - DACHS_KWD("end")) % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        if_stmt
            = (
                DACHS_KWD(if_kind) >> (typed_expr - DACHS_KWD("then")) >> (DACHS_KWD("then") || sep)
                >> if_then_stmt_block >> -sep
                >> *(
                    qi::as<ast::node_type::if_stmt::elseif_type>()[
                        DACHS_KWD("elseif") >> (typed_expr - DACHS_KWD("then")) >> (DACHS_KWD("then") || sep)
                        >> if_then_stmt_block >> -sep
                    ]
                ) >> -(DACHS_KWD("else") >> -sep >> if_else_stmt_block >> -sep)
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::if_stmt>(_1, _2, _3, _4, _5)
            ];

        return_stmt
            = (
                DACHS_KWD("ret") >> -(typed_expr % comma)
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(as_vector(_1))
            ];

        case_when_stmt_block
            = (
                *((compound_stmt - DACHS_KWD("end") - DACHS_KWD("else") - DACHS_KWD("when")) >> sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        case_stmt
            = (
                "case" >> sep
                >> +(
                    qi::as<ast::node_type::case_stmt::when_type>()[
                        DACHS_KWD("when") >> (typed_expr - DACHS_KWD("then")) >> (DACHS_KWD("then") || sep)
                        >> case_when_stmt_block
                    ]
                ) >> -(
                    DACHS_KWD("else") >> -sep >> stmt_block_before_end >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::case_stmt>(_1, _2)
            ];

        switch_stmt
            = (
                DACHS_KWD("case") >> typed_expr >> sep
                >> +(
                    qi::as<ast::node_type::switch_stmt::when_type>()[
                        DACHS_KWD("when") >> (typed_expr - DACHS_KWD("then")) % comma >> (DACHS_KWD("then") || sep)
                        >> case_when_stmt_block
                    ]
                ) >> -(
                    DACHS_KWD("else") >> -sep >> stmt_block_before_end >> -sep
                ) >> "end"
            ) [
                _val = make_node_ptr<ast::node::switch_stmt>(_1, _2, _3)
            ];

        for_stmt
            = (
                // Note: "do" might colide with do-end block in typed_expr
                DACHS_KWD("for") >> (parameter - DACHS_KWD("in")) % comma >> DACHS_KWD("in") >> typed_expr >> sep
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::for_stmt>(_1, _2, _3)
            ];

        while_stmt
            = (
                // Note: "do" might colide with do-end block in typed_expr
                DACHS_KWD("for") >> typed_expr >> (DACHS_KWD("do") || sep)
                >> stmt_block_before_end >> -sep
                >> "end"
            ) [
                _val = make_node_ptr<ast::node::while_stmt>(_1, _2)
            ];

        postfix_if_return_stmt
            = (
                DACHS_KWD("ret") >> -((typed_expr - DACHS_KWD(if_kind)) % comma)
            ) [
                _val = make_node_ptr<ast::node::return_stmt>(as_vector(_1))
            ];

        postfix_if_stmt
            = (
                (
                    postfix_if_return_stmt
                  | assignment_stmt
                  | (typed_expr - DACHS_KWD(if_kind))
                )
                >> DACHS_KWD(if_kind) >> typed_expr
            ) [
                _val = make_node_ptr<ast::node::postfix_if_stmt>(_1, _2, _3)
            ];

        do_stmt
            = (
                DACHS_KWD("do")
                >> -qi::eol >> stmt_block_before_end >> -sep
                >> DACHS_KWD("end")
            );

        compound_stmt
            = (
                  if_stmt
                | case_stmt
                | switch_stmt
                | for_stmt
                | while_stmt
                | do_stmt
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

        func_body_stmt_block
            = (
                -((compound_stmt - DACHS_KWD("ensure") - DACHS_KWD("end")) % sep)
            ) [
                _val = make_node_ptr<ast::node::statement_block>(_1)
            ];

        function_definition
            = (
                DACHS_KWD(func_kind) > func_def_name > function_param_decls > -((':' >> -qi::eol) > qualified_type) > sep
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
                _val = make_node_ptr<ast::node::variable_decl>(false, _1, _2)
            ];

        constant_definition
            = (
                constant_decl % comma >> trailing_comma >> ":=" >> -qi::eol >> typed_expr % comma
            ) [
                _val = make_node_ptr<ast::node::initialize_stmt>(_1, _2)
            ];

        class_name
            = (
                qi::lexeme[
                    (qi::alpha | '_'_p)[_val += _1]
                    >> *(alnum | '_'_p)[_val += _1]
                ]
            );

        access_specifier
            = (
                qi::eps[_val = true]
                >> -(
                    "+"_l[_val = true]
                  | "-"_l[_val = false]
                )
            );

        instance_variable_decl
            = (
                variable_name >> -(
                    (-qi::eol >> ':') > -qi::eol > qualified_type
                )
            ) [
                _val = make_node_ptr<ast::node::variable_decl>(true, _1, _2, qi::_r1)
            ];

        instance_variable_decls
            = (
                qi::omit[access_specifier[_a = _1]]
                >> (
                    instance_variable_decl(_a)
                ) % (-qi::eol >> ',')
            );

        method_definition
            = (
                access_specifier[_a = _1] >> function_definition[_val = _1]
            )[
                phx::bind(
                    [](auto const& node, bool const is_public)
                    {
                        if (!node) {
                            return;
                        }

                        node->accessibility = is_public;
                    }
                  , _val
                  , _a
                )
            ];

        constructor
            = (
                DACHS_KWD("init") > function_param_decls > sep
                > stmt_block_before_end > -sep
                > "end"
            ) [
                _val = make_node_ptr<ast::node::function_definition>(ast::node_type::function_definition::ctor_tag{}, _1, _2)
            ];

        class_definition
            = (
                DACHS_KWD("class") > class_name
                > *(
                    sep >> (
                        method_definition[
                            phx::push_back(_b, _1)
                        ]
                      | constructor[
                            phx::push_back(_b, _1)
                        ]
                      | (instance_variable_decls - "end")[
                            phx::insert(_a, phx::end(_a), phx::begin(_1), phx::end(_1))
                        ]
                    )
                ) > sep > "end"
            )[
                _val = make_node_ptr<ast::node::class_definition>(_1, _a, _b)
            ];

        import
            = (
                    DACHS_KWD("import")
                    > (
                        +(alnum | '_'_p)[_a += _1] % '.'_p[_a += _1]
                    )
            ) [
                _val = make_node_ptr<ast::node::import>(_a)
            ];

    #undef DACHS_KWD

        // Set callback to get the position of node and show obvious compile error {{{
        if (!check_only) {

            detail::set_position_getter_on_success(

                // _val : parsed value
                // _1   : position before parsing
                // _2   : end of string to parse
                // _3   : position after parsing
                phx::bind(
                    detail::position_getter<Iterator>{code_begin}
                    , _val, _1, _3)

                , inu
                , primary_literal
                , array_literal
                , tuple_literal
                , lambda_expr
                , let_expr
                , symbol_literal
                , dict_literal
                , var_ref
                , var_ref_before_space
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
                , variable_decl_without_init
                , initialize_stmt
                , assignment_stmt
                , postfix_if_return_stmt
                , postfix_if_stmt
                , do_stmt
                , function_definition
                , constructor
                , class_definition
                , constant_decl
                , constant_definition
                , do_block
                , stmt_block_before_end
                , if_then_stmt_block
                , if_else_stmt_block
                , case_when_stmt_block
                , func_body_stmt_block
                , lambda_expr_oneline
                , lambda_expr_do_end
                , method_definition
                , instance_variable_decl
                , import
            );

            // Note:
            // begin_end_expr and let_expr generates multiple nodes.
            // Source location should be set to all generated nodes.
            detail::set_position_getter_on_success(
                    phx::bind(
                        [code_begin](auto const& invocation, Iterator const before, Iterator const after)
                        {
                            auto const d = std::distance(before.base(), after.base());
                            invocation->line = spirit::get_line(before);
                            invocation->col = spirit::get_column(code_begin, before);
                            invocation->length = d < 0 ? 0 : d;

                            if (invocation->is_begin_end || invocation->is_let) {
                                auto const lambda
                                    = helper::variant::get_as<ast::node::lambda_expr>(invocation->child);
                                assert(lambda);
                                (*lambda)->set_source_location(*invocation);
                                (*lambda)->def->set_source_location(*invocation);
                            }
                        },
                        _val,
                        _1,
                        _3
                    ),
                    let_expr,
                    begin_end_expr
                );

        } // if (!check_only) {

        qi::on_error<qi::fail>(
            inu,
            // _1 : begin of string to parse
            // _2 : end of string to parse
            // _3 : iterator at failed point
            // _4 : what failed?
            std::cerr << phx::val(c.red("Error") + " in ")
                      << phx::bind([](auto const begin, auto const err_pos) {
                              return (boost::format("line:%1%, col:%2%") % spirit::get_line(err_pos) % spirit::get_column(begin, err_pos)).str();
                          }, _1, _3) << '\n'
                      << c.bold("Expected ", false) << _4 << c.reset()
                      << "\n\n"
                      << phx::bind([this /*for 'c'*/](auto const begin, auto const end, auto const err_itr) {
                              return std::string{
                                        spirit::get_line_start(begin, err_itr),
                                        std::find_if(err_itr, end, [](auto c){ return c == '\r' || c == '\n'; })
                                     } + '\n'
                                     + std::string(spirit::get_column(begin, err_itr)-1, ' ') + c.green("^ here");
                          }, _1, _2, _3)
                      << '\n' << std::endl
        );
        // }}}

        // Rule names {{{
        inu.name("program");
        primary_literal.name("primary literal");
        integer_literal.name("integer literal");
        uinteger_literal.name("unsigned integer literal");
        character_literal.name("character literal");
        float_literal.name("float literal");
        boolean_literal.name("boolean literal");
        string_literal.name("string literal");
        array_literal.name("array literal");
        tuple_literal.name("tuple literal");
        lambda_expr.name("lambda expression");
        begin_end_expr.name("begin-end expression");
        let_expr.name("let-in expression");
        symbol_literal.name("symbol literal");
        dict_literal.name("dictionary literal");
        var_ref.name("variable reference");
        var_ref.name("variable reference before space");
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
        variable_decl_without_init.name("variable declaration without initialization");
        initialize_stmt.name("initialize statement");
        assignment_stmt.name("assignment statement");
        do_stmt.name("do statement");
        compound_stmt.name("compound statement");
        function_definition.name("function definition");
        constructor.name("constructor");
        class_definition.name("class definition");
        constant_decl.name("constant declaration");
        constant_definition.name("constant definition");
        do_block.name("do-end block");
        sep.name("separater");
        constructor_call.name("constructor call");
        function_param_decls.name("parameter declarations of function");
        stmt_block_before_end.name("statements block before 'end'");
        if_then_stmt_block.name("'then' clause in if statement");
        if_else_stmt_block.name("'else' clause in if statement");
        case_when_stmt_block.name("'when' clause in case statement");
        func_body_stmt_block.name("statements in body of function");
        called_function_name.name("name of called function");
        function_name.name("name of function");
        func_def_name.name("name of function definition");
        variable_name.name("variable name");
        type_name.name("type name");
        postfix_if_return_stmt.name("return statement in postfix if statement");
        lambda_expr_oneline.name("\"in\" lambda expression");
        lambda_expr_do_end.name("\"do-end\" lambda expression");
        instance_variable_decl.name("declaration of instance variable");
        instance_variable_decls.name("declarations of instance variable");
        method_definition.name("member function definition");
        class_name.name("name of class");
        access_specifier.name("access_specifier");
        import.name("import module");
        // }}}
    }

    ~dachs_grammar()
    {}

private:

    // Rules {{{
    rule<qi::unused_type()> sep, comma, trailing_comma;

#define DACHS_DEFINE_RULE(n) rule<ast::node::n()> n
#define DACHS_DEFINE_RULE_WITH_LOCALS(n, ...) rule<ast::node::n(), qi::locals< __VA_ARGS__ >> n

    DACHS_DEFINE_RULE_WITH_LOCALS(
            inu,
            std::vector<ast::node::function_definition>,
            std::vector<ast::node::initialize_stmt>,
            std::vector<ast::node::class_definition>,
            std::vector<ast::node::import>
        );
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
    DACHS_DEFINE_RULE_WITH_LOCALS(class_definition, std::vector<ast::node::variable_decl>, std::vector<ast::node::function_definition>);
    DACHS_DEFINE_RULE_WITH_LOCALS(import, std::string);

    rule<char()> character_literal;
    rule<double()> float_literal;
    rule<int()> integer_literal;
    rule<unsigned int()> uinteger_literal;
    rule<bool()> boolean_literal;
    rule<std::string()> string_literal;
    rule<ast::node::variable_decl()> constant_decl, variable_decl_without_init;
    rule<ast::node::initialize_stmt()> constant_definition;
    rule<ast::node::function_definition()> do_block, lambda_expr_do_end, constructor;
    rule<ast::node::statement_block()> do_stmt;
    rule<bool()> access_specifier;
    rule<ast::node::function_definition(), qi::locals<bool>> method_definition;
    rule<std::vector<ast::node::variable_decl>(), qi::locals<bool>> instance_variable_decls;
    rule<ast::node::variable_decl(bool)> instance_variable_decl;

    rule<ast::node::any_expr()>
          primary_literal
        , array_literal
        , dict_literal
        , lambda_expr
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
        , var_ref_before_space
    ;

    rule<ast::node::func_invocation()> begin_end_expr;
    rule<ast::node::func_invocation(), qi::locals<ast::node::statement_block>> let_expr;

    rule<ast::node::any_expr(), qi::locals<std::vector<ast::node::any_expr>>> tuple_literal;
    rule<ast::node::any_expr(), qi::locals<std::string>> symbol_literal;
    rule<
        ast::node::function_definition(),
        qi::locals<
            std::vector<ast::node::parameter>,
            std::vector<ast::node::parameter>
        >
    >  lambda_expr_oneline;

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
    rule<std::string()> called_function_name
                      , function_name
                      , func_def_name
                      , variable_name
                      , type_name
                      , unary_operator
                      , binary_operator
                      , assign_operator
                      , class_name
                    ;
    decltype(return_stmt) postfix_if_return_stmt;

#undef DACHS_DEFINE_RULE
#undef DACHS_DEFINE_RULE_WITH_LOCALS
    // }}}

    // Symbol tables {{{
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

ast::ast parser::parse(std::string const& code, std::string const& file_name) const
{
    auto itr = detail::line_pos_iterator(std::begin(code));
    using iterator_type = decltype(itr);
    auto const begin = itr;
    auto const end = detail::line_pos_iterator(std::end(code));
    dachs_grammar<iterator_type> dachs_parser(begin, false);
    comment_skipper<iterator_type> skipper;
    ast::node::inu root;

    if (!qi::phrase_parse(itr, end, dachs_parser, skipper, root) || itr != end) {
        throw parse_error{spirit::get_line(itr), spirit::get_column(begin, itr)};
    }

    return {root, file_name};
}

void parser::check_syntax(std::string const& code) const
{
    auto itr = detail::line_pos_iterator(std::begin(code));
    using iterator_type = decltype(itr);
    auto const begin = itr;
    auto const end = detail::line_pos_iterator(std::end(code));
    dachs_grammar<iterator_type> dachs_parser(begin, true);
    comment_skipper<iterator_type> skipper;
    ast::node::inu root;

    if (!qi::phrase_parse(itr, end, dachs_parser, skipper, root) || itr != end) {
        throw parse_error{spirit::get_line(itr), spirit::get_column(begin, itr)};
    }
}

} // namespace syntax
} // namespace dachs
