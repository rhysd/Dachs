#if !defined DACHS_COMMENT_SKIP_PARSER_HPP_INCLUDED
#define      DACHS_COMMENT_SKIP_PARSER_HPP_INCLUDED

#include <boost/spirit/include/qi.hpp>

namespace dachs {
namespace syntax {

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;

template<class Iterator>
class comment_skipper final : public qi::grammar<Iterator> {
public:
    comment_skipper() noexcept
        : comment_skipper::base_type(comment)
    {
        comment
            = (
                ascii::blank
              | (
                  "#{" >> *(
                      "\\}" | (
                          (qi::char_ - (qi::lexeme['}' >> &(qi::lit('#'))]))
                        )
                    ) >> "}#"
              ) | (
                    '#' >> *(qi::char_ - qi::eol)
              )
            );
    }
private:
    qi::rule<Iterator> comment;
};

} // namespace syntax
} // namespace dachs

#endif    // DACHS_COMMENT_SKIP_PARSER_HPP_INCLUDED
