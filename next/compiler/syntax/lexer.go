package syntax

import (
	"bytes"
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"io"
	"unicode"
	"unicode/utf8"
)

type stateFn func(*Lexer) stateFn

const eof = -1

type Lexer struct {
	state   stateFn
	start   prelude.Pos
	current prelude.Pos
	src     *prelude.Source
	input   *bytes.Reader
	Tokens  chan *Token
	top     rune
	eof     bool
	Error   func(*prelude.Error)
}

func NewLexer(src *prelude.Source) *Lexer {
	start := prelude.Pos{
		Offset: 0,
		Line:   1,
		Column: 1,
		File:   src,
	}
	return &Lexer{
		state:   lex,
		start:   start,
		current: start,
		input:   bytes.NewReader(src.Code),
		src:     src,
		Tokens:  make(chan *Token),
		Error:   nil,
	}
}

func (l *Lexer) Lex() {
	l.forward()
	for l.state != nil {
		l.state = l.state(l)
	}
}

func (l *Lexer) forward() {
	r, _, err := l.input.ReadRune()
	if err == io.EOF {
		l.top = 0
		l.eof = true
		return
	}

	if err != nil {
		panic(err)
	}

	if !utf8.ValidRune(r) {
		// TODO: Do not panic
		panic(fmt.Errorf("Invalid UTF-8 character at line:%d,col:%d: '%c' (%d)", l.current.Line, l.current.Column, r, r))
	}

	l.top = r
	l.eof = false
}

func (l *Lexer) eat() {
	size := utf8.RuneLen(l.top)
	l.current.Offset += size

	// TODO: Consider \n\r
	if l.top == '\n' {
		l.current.Line++
		l.current.Column = 1
	} else {
		l.current.Column += size
	}

	l.forward()
}

func (l *Lexer) consume() {
	if l.eof {
		return
	}
	l.eat()
	l.start = l.current
}

func isLetter(r rune) bool {
	return 'a' <= r && r <= 'z' ||
		'A' <= r && r <= 'Z' ||
		r == '_' ||
		r >= utf8.RuneSelf && unicode.IsLetter(r)
}

func (l *Lexer) eatIdent() bool {
	if !isLetter(l.top) {
		l.expected("letter for head character of identifer", l.top)
		return false
	}
	l.eat()

	for isLetter(l.top) || unicode.IsDigit(l.top) {
		l.eat()
	}
	return true
}

func (l *Lexer) emit(kind TokenKind) {
	l.Tokens <- &Token{
		kind,
		l.start,
		l.current,
	}
	l.start = l.current
}

func (l *Lexer) fail(msg string) {
	if l.Error == nil {
		return
	}
	l.Error(prelude.NewError(l.start, l.current, msg))
}

func (l *Lexer) expected(s string, actual rune) {
	l.fail(fmt.Sprintf("Expected %s but got '%c'(%d)", s, actual, actual))
	l.emit(TokenIllegal)
}

func (l *Lexer) currentString() string {
	return string(l.src.Code[l.start.Offset:l.current.Offset])
}

func lexExclamation(l *Lexer) stateFn {
	// Eat first '!'
	l.eat()

	switch l.top {
	case '=':
		// Lex !=
		l.eat()
		l.emit(TokenNotEqual)
	case '!':
		// Lex comment
		for l.top != '\n' && !l.eof {
			l.eat()
		}
		l.emit(TokenComment)
	default:
		l.emit(TokenNot)
	}

	return lex
}

var keywords = map[string]TokenKind{
	"end":    TokenEnd,
	"if":     TokenIf,
	"then":   TokenThen,
	"else":   TokenElse,
	"case":   TokenCase,
	"match":  TokenMatch,
	"ret":    TokenRet,
	"import": TokenImport,
	"type":   TokenType,
	"of":     TokenOf,
	"for":    TokenFor,
	"in":     TokenIn,
	"typeof": TokenTypeof,
	"as":     TokenAs,
	"func":   TokenFunc,
	"do":     TokenDo,
	"true":   TokenBool,
	"false":  TokenBool,
}

func lexIdent(l *Lexer) stateFn {
	if l.top == '\'' {
		// For type variable 'a
		l.eat()
	}

	if !l.eatIdent() {
		return nil
	}

	tok, ok := keywords[l.currentString()]
	if ok {
		l.emit(tok)
	} else {
		l.emit(TokenIdent)
	}

	return lex
}

func lexStringLiteral(l *Lexer) stateFn {
	l.eat() // Eat first '"'
	for !l.eof {
		if l.top == '\\' {
			// Eat '\' and second character
			l.eat()
			l.eat()
		}
		if l.top == '"' {
			l.eat()
			l.emit(TokenString)
			return lex
		}
		l.eat()
	}
	l.fail("Unclosed string literal")
	l.emit(TokenIllegal)
	return nil
}

func lexEqual(l *Lexer) stateFn {
	l.eat()

	switch l.top {
	case '=':
		l.eat()
		l.emit(TokenEqual)
	default:
		l.emit(TokenAssign)
	}

	return lex
}

func lexColon(l *Lexer) stateFn {
	l.eat()

	switch l.top {
	case '=':
		// Lex :=
		l.eat()
		l.emit(TokenDefine)
	case ':':
		// Lex ::
		l.eat()
		l.emit(TokenColonColon)
	case '>':
		// Lex =>
		l.eat()
		l.emit(TokenFatRightArrow)
	default:
		l.emit(TokenColon)
	}

	return lex
}

func lexDigit(l *Lexer) stateFn {
	tok := TokenInt

	// Eat first digit
	l.eat()
	for unicode.IsDigit(l.top) {
		l.eat()
	}

	// Note: Allow 1. as 1.0
	if l.top == '.' {
		tok = TokenFloat
		l.eat()
		for unicode.IsDigit(l.top) {
			l.eat()
		}
	}

	if l.top == 'e' || l.top == 'E' {
		tok = TokenFloat
		l.eat()
		if l.top == '+' || l.top == '-' {
			l.eat()
		}
		if !unicode.IsDigit(l.top) {
			l.expected("number for exponential part of float literal", l.top)
			return nil
		}
		for unicode.IsDigit(l.top) {
			l.eat()
		}
	}

	l.emit(tok)
	return lex
}

func lex(l *Lexer) stateFn {
	for {
		if l.eof {
			l.emit(TokenEOF)
			return nil
		}
		switch l.top {
		case '!':
			return lexExclamation
		case '\n':
			l.eat()
			l.emit(TokenNewline)
		case ';':
			l.eat()
			l.emit(TokenSemicolon)
		case '(':
			l.eat()
			l.emit(TokenLParen)
		case ')':
			l.eat()
			l.emit(TokenRParen)
		case '{':
			l.eat()
			l.emit(TokenLBrace)
		case '}':
			l.eat()
			l.emit(TokenRBrace)
		case '[':
			l.eat()
			l.emit(TokenLBracket)
		case ']':
			l.eat()
			l.emit(TokenRBracket)
		case '\'':
			// Type variable 'a
			return lexIdent
		case '"':
			return lexStringLiteral
		case '-':
			l.eat()
			if l.top == '>' {
				l.eat()
				l.emit(TokenRightArrow)
			} else {
				l.emit(TokenMinus)
			}
		case '+':
			l.eat()
			l.emit(TokenPlus)
		case '*':
			l.eat()
			l.emit(TokenStar)
		case '/':
			l.eat()
			l.emit(TokenDiv)
		case '|':
			l.eat()
			if l.top != '|' {
				l.expected("'|' for '||'", l.top)
				return nil
			}
			l.eat()
			l.emit(TokenOr)
		case '&':
			l.eat()
			if l.top != '&' {
				l.expected("'&' for '&&'", l.top)
				return nil
			}
			l.eat()
			l.emit(TokenAnd)
		case '%':
			l.eat()
			l.emit(TokenPercent)
		case '=':
			return lexEqual
		case ':':
			return lexColon
		case '<':
			l.eat()
			if l.top == '=' {
				l.eat()
				l.emit(TokenLessEqual)
			} else {
				l.emit(TokenLess)
			}
		case '>':
			l.eat()
			if l.top == '=' {
				l.eat()
				l.emit(TokenGreaterEqual)
			} else {
				l.emit(TokenGreater)
			}
		default:
			switch {
			case unicode.IsSpace(l.top):
				// Note: '\n' was already checked
				l.consume()
			case unicode.IsDigit(l.top):
				return lexDigit
			default:
				return lexIdent
			}
		}
	}
}
