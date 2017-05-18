package syntax

import (
	"bytes"
	"fmt"
	"io"
	"unicode"
	"unicode/utf8"

	"github.com/rhysd/Dachs/next/compiler/prelude"
)

type stateFn func(*Lexer) stateFn

const eof = -1

// Lexer lexes Dachs source code in another goroutine
type Lexer struct {
	state   stateFn
	start   prelude.Pos
	current prelude.Pos
	src     *prelude.Source
	input   *bytes.Reader
	// Tokens is a channel to receive tokens lexed by lexer
	Tokens chan *Token
	top    rune
	eof    bool
	// Error is a callback called when lexer find an error
	Error func(*prelude.Error)
}

// NewLexer makes a new lexer instance
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

// Lex starts to lex target source. Tokens will be sent to Tokens channel
func (l *Lexer) Lex() {
	prelude.Log("Start lexing:", l.src)
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
		l.fail(err.Error())
		l.eof = true
		return
	}

	if !utf8.ValidRune(r) {
		l.fail(fmt.Sprintf("Invalid UTF-8 character at line:%d,col:%d: '%c' (%d)", l.current.Line, l.current.Column, r, r))
		l.eof = true
		return
	}

	l.top = r
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

func isDigit(r rune) bool {
	return '0' <= r && r <= '9'
}

func isHex(r rune) bool {
	return '0' <= r && r <= '9' ||
		'a' <= r && r <= 'f' ||
		'A' <= r && r <= 'F'
}

func (l *Lexer) eatIdent() bool {
	if !isLetter(l.top) {
		l.expected("letter for head character of identifer", l.top)
		return false
	}
	l.eat()

	for isLetter(l.top) || isDigit(l.top) {
		l.eat()
	}
	return true
}

func (l *Lexer) emit(kind TokenKind) {
	t := &Token{
		kind,
		l.start,
		l.current,
	}
	l.Tokens <- t
	l.start = l.current
	prelude.Log("Lex token:", t.String())
}

func (l *Lexer) fail(msg string) {
	if l.Error != nil {
		l.Error(prelude.NewError(l.start, l.current, msg))
	}
	l.emit(TokenIllegal)
}

func (l *Lexer) expected(s string, actual rune) {
	l.fail(fmt.Sprintf("Expected %s but got '%c'(%d)", s, actual, actual))
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
	"for":    TokenFor,
	"in":     TokenIn,
	"typeof": TokenTypeof,
	"as":     TokenAs,
	"func":   TokenFunc,
	"do":     TokenDo,
	"true":   TokenBool,
	"false":  TokenBool,
	"break":  TokenBreak,
	"next":   TokenNext,
	"var":    TokenVar,
	"let":    TokenLet,
	"switch": TokenSwitch,
	"with":   TokenWith,
}

func lexIdent(l *Lexer) stateFn {
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
	return nil
}

func lexEqual(l *Lexer) stateFn {
	l.eat()

	switch l.top {
	case '=':
		l.eat()
		l.emit(TokenEqual)
	case '>':
		// Lex =>
		l.eat()
		l.emit(TokenFatRightArrow)
	default:
		l.emit(TokenAssign)
	}

	return lex
}

func lexColon(l *Lexer) stateFn {
	l.eat()

	if l.top == ':' {
		// Lex ::
		l.eat()
		l.emit(TokenColonColon)
	} else {
		l.emit(TokenColon)
	}

	return lex
}

func lexDigit(l *Lexer) stateFn {
	tok := TokenInt

	// Note:
	// This function assumes first digit is already eaten

	for isDigit(l.top) {
		l.eat()
	}

	// Note: Allow 1. as 1.0
	if l.top == '.' {
		tok = TokenFloat
		l.eat()
		for isDigit(l.top) {
			l.eat()
		}
	}

	if l.top == 'e' || l.top == 'E' {
		tok = TokenFloat
		l.eat()
		if l.top == '+' || l.top == '-' {
			l.eat()
		}
		if !isDigit(l.top) {
			l.expected("number for exponential part of float literal", l.top)
			return nil
		}
		for isDigit(l.top) {
			l.eat()
		}
	}

	if tok == TokenInt && l.top == 'u' {
		// Lex 'u' of 123u
		l.eat()
	}

	l.emit(tok)
	return lex
}

func lexHex(l *Lexer) stateFn {
	if !isHex(l.top) {
		l.expected("hexiadecimal number (0~f)", l.top)
		return nil
	}
	l.eat()

	for isHex(l.top) {
		l.eat()
	}

	if l.top == 'u' {
		l.eat()
	}

	l.emit(TokenInt)
	return lex
}

func lexBin(l *Lexer) stateFn {
	if l.top != '0' && l.top != '1' {
		l.expected("binary number (0 or 1)", l.top)
		return nil
	}
	l.eat()

	for l.top == '0' || l.top == '1' {
		l.eat()
	}

	if l.top == 'u' {
		l.eat()
	}

	l.emit(TokenInt)
	return lex
}

func lexNumber(l *Lexer) stateFn {
	if l.top != '0' {
		l.eat()
		return lexDigit
	}

	l.eat()
	switch l.top {
	case 'x':
		l.eat()
		return lexHex
	case 'b':
		l.eat()
		return lexBin
	default:
		return lexDigit
	}
}

func lexDot(l *Lexer) stateFn {
	// Eat first dot
	l.eat()

	if l.top != '.' {
		l.emit(TokenDot)
		return lex
	}

	l.eat()
	if l.top != '.' {
		l.expected("'.' for '...'", l.top)
		return nil
	}

	l.eat()
	l.emit(TokenEllipsis)
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
			l.eat()
			l.emit(TokenSingleQuote)
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
		case ',':
			l.eat()
			l.emit(TokenComma)
		case '.':
			return lexDot
		default:
			switch {
			case unicode.IsSpace(l.top):
				// Note: '\n' was already checked
				l.consume()
			case isDigit(l.top):
				return lexNumber
			default:
				return lexIdent
			}
		}
	}
}
