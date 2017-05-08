package syntax

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)

// TokenKind represents kind of tokens for Dachs.
type TokenKind int

const (
	TokenIllegal TokenKind = iota
	TokenComment
	TokenNewline
	TokenSemicolon
	TokenLParen
	TokenRParen
	TokenLBrace
	TokenRBrace
	TokenLBracket
	TokenRBracket
	TokenIdent
	TokenBool
	TokenInt
	TokenFloat
	TokenString
	TokenMinus
	TokenPlus
	TokenStar
	TokenDiv
	TokenNot
	TokenOr
	TokenAnd
	TokenPercent
	TokenEqual
	TokenNotEqual
	TokenAssign
	TokenLess
	TokenLessEqual
	TokenGreater
	TokenGreaterEqual
	TokenEnd
	TokenIf
	TokenThen
	TokenElse
	TokenCase
	TokenMatch
	TokenRet
	TokenImport
	TokenDot
	TokenType
	TokenColon
	TokenFor
	TokenIn
	TokenFatRightArrow
	TokenColonColon
	TokenTypeof
	TokenAs
	TokenFunc
	TokenDo
	TokenRightArrow
	TokenComma
	TokenBreak
	TokenNext
	TokenEllipsis
	TokenSingleQuote
	TokenVar
	TokenLet
	TokenWhen
	TokenEOF
)

var tokenTable = [...]string{
	TokenIllegal:       "ILLEGAL",
	TokenComment:       "COMMENT",   // !!
	TokenNewline:       "NEWLINE",   // \n
	TokenSemicolon:     "SEMICOLON", // ;
	TokenLParen:        "LPAREN",    // (
	TokenRParen:        "RPAREN",    // )
	TokenLBrace:        "LBRACE",    // {
	TokenRBrace:        "RBRACE",    // }
	TokenLBracket:      "LBRACKET",  // [
	TokenRBracket:      "RBRACKET",  // ]
	TokenIdent:         "IDENT",     // Including `'a`, `_`
	TokenBool:          "BOOL",
	TokenInt:           "INT",
	TokenFloat:         "FLOAT",
	TokenString:        "STRING",        // "...",
	TokenMinus:         "MINUS",         // -
	TokenPlus:          "PLUS",          // +
	TokenStar:          "STAR",          // *
	TokenDiv:           "DIV",           // /
	TokenNot:           "NOT",           // !
	TokenOr:            "OR",            // ||
	TokenAnd:           "AND",           // &&
	TokenPercent:       "PERCENT",       // %
	TokenEqual:         "EQUAL",         // ==
	TokenNotEqual:      "NOT_EQUAL",     // !=
	TokenAssign:        "ASSIGN",        // =
	TokenLess:          "LESS",          // <
	TokenLessEqual:     "LESS_EQUAL",    // <=
	TokenGreater:       "GREATER",       // >
	TokenGreaterEqual:  "GREATER_EQUAL", // >=
	TokenEnd:           "END",
	TokenIf:            "IF",
	TokenThen:          "THEN",
	TokenElse:          "ELSE",
	TokenCase:          "CASE",
	TokenMatch:         "MATCH",
	TokenRet:           "RET",
	TokenImport:        "IMPORT",
	TokenDot:           "DOT",
	TokenType:          "TYPE",
	TokenColon:         "COLON", // :
	TokenFor:           "FOR",
	TokenIn:            "IN",
	TokenFatRightArrow: "FAT_RIGHT_ARROW", // =>
	TokenColonColon:    "COLON_COLON",     // ::
	TokenTypeof:        "TYPEOF",
	TokenAs:            "AS",
	TokenFunc:          "FUNC",
	TokenDo:            "DO",
	TokenRightArrow:    "RIGHT_ARROW", // ->
	TokenComma:         "COMMA",
	TokenBreak:         "BREAK",
	TokenNext:          "NEXT",
	TokenEllipsis:      "ELLIPSIS",
	TokenSingleQuote:   "SINGLE_QUOTE",
	TokenVar:           "VAR",
	TokenLet:           "LET",
	TokenWhen:          "WHEN",
	TokenEOF:           "EOF",
}

func (k TokenKind) String() string {
	return tokenTable[k]
}

// Token represents tokens lexed from source code
type Token struct {
	Kind  TokenKind
	Start prelude.Pos
	End   prelude.Pos
}

func (tok *Token) String() string {
	return fmt.Sprintf(
		"<%s:%s>(%d:%d:%d-%d:%d:%d)",
		tokenTable[tok.Kind],
		tok.Value(),
		tok.Start.Line, tok.Start.Column, tok.Start.Offset,
		tok.End.Line, tok.End.Column, tok.End.Offset)
}

// Value strips the code snippet corresponding to the token
func (tok *Token) Value() string {
	return string(tok.Start.File.Code[tok.Start.Offset:tok.End.Offset])
}
