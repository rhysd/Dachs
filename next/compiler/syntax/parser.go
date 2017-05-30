package syntax

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)

type pseudoLexer struct {
	tokens      chan *Token
	errors      []string
	result      *ast.Program
	skipNewline bool
	lastToken   *Token
}

func (l *pseudoLexer) Lex(lval *yySymType) int {
	for {
		select {
		case t := <-l.tokens:
			prelude.Log("Parser received token:", t.Kind.String())
			lval.token = t

			switch t.Kind {
			case TokenEOF:
				prelude.Log("Received EOF. Parser quitting")
				// Zero means that the input ends
				// (see golang.org/x/tools/cmd/goyacc/testdata/expr/expr.y)
				return 0
			case TokenComment:
				continue
			case TokenIllegal:
				prelude.Log("Received ILLEGAL. Parser quitting")
				return 0
			case TokenNewline:
				if l.skipNewline {
					prelude.Log("skipNewline flag is enabled. Skipping newline")
					continue
				}
			}

			l.skipNewline = false
			l.lastToken = t

			// XXX:
			// Converting token value into yacc's token.
			// This conversion requires that token order must the same as
			// yacc's token order. EOF is a first token. So we can use it
			// to make an offset between token value and yacc's token value.
			return int(t.Kind) + ILLEGAL
		}
	}
}

func (l *pseudoLexer) Error(msg string) {
	l.errors = append(l.errors, msg)
}

func (l *pseudoLexer) getError() error {
	msg := fmt.Sprintf("Parse error: " + l.errors[0])
	if l.lastToken != nil {
		err := prelude.NewErrorAt(l.lastToken.Start, msg)
		for _, e := range l.errors[1:] {
			err = err.Note(e)
		}
		return err
	} else {
		return fmt.Errorf(msg)
	}
}

func Parse(src *prelude.Source) (*ast.Program, error) {
	prelude.Log("Start parsing source:", src)
	var lexErr error
	l := NewLexer(src)
	l.Error = func(err *prelude.Error) {
		lexErr = err
	}
	go l.Lex()
	prog, err := ParseTokens(l.Tokens)
	if lexErr != nil {
		return nil, lexErr
	}
	return prog, err
}

func ParseTokens(tokens chan *Token) (*ast.Program, error) {
	if prelude.IsLogEnabled(prelude.Parsing) {
		prelude.Log("yyDebug is enabled: 9999")
		yyDebug = 9999
	}

	yyErrorVerbose = true

	l := &pseudoLexer{tokens: tokens}
	ret := yyParse(l)

	if ret != 0 || len(l.errors) > 0 {
		prelude.Logf("Parser failed. ret: %d, error count: %d", ret, len(l.errors))
		return nil, l.getError()
	}

	if l.result == nil {
		panic("FATAL: No error detected but result is nil")
	}

	prelude.Log("Parsed source successfully:", l.result.File())

	return l.result, nil
}
