package syntax

import (
	"bytes"
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)

type pseudoLexer struct {
	tokens      chan *Token
	errCount    int
	errMessage  bytes.Buffer
	result      *ast.Program
	skipNewline bool
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
	l.errCount++
	l.errMessage.WriteString(fmt.Sprintf("  * %s\n", msg))
}

func (l *pseudoLexer) getError() error {
	return fmt.Errorf("%d error(s) while parsing\n%s", l.errCount, l.errMessage.String())
}

func Parse(src *prelude.Source) (*ast.Program, error) {
	var lexErr error
	l := NewLexer(src)
	l.Error = func(err *prelude.Error) {
		lexErr = err
	}
	defer close(l.Tokens)
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

	if ret != 0 || l.errCount != 0 {
		prelude.Logf("Parser failed. ret: %d, error count: %d", ret, l.errCount)
		return nil, l.getError()
	}

	if l.result == nil {
		panic("FATAL: No error detected but result is nil")
	}

	prelude.Log("Parsed source successfully:", l.result.File().Name)

	return l.result, nil
}
