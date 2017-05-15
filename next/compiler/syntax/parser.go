package syntax

import (
	"bytes"
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
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
			lval.token = t

			switch t.Kind {
			case TokenEOF:
				// Zero means that the input ends
				// (see golang.org/x/tools/cmd/goyacc/testdata/expr/expr.y)
				return 0
			case TokenComment:
				continue
			case TokenIllegal:
				return 0
			case TokenNewline:
				if l.skipNewline {
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

func Parse(tokens chan *Token) (*ast.Program, error) {
	// yyDebug = 9999
	yyErrorVerbose = true

	l := &pseudoLexer{tokens: tokens}
	ret := yyParse(l)

	if ret != 0 || l.errCount != 0 {
		return nil, l.getError()
	}

	if l.result == nil {
		return nil, fmt.Errorf("Parsing failed")
	}

	return l.result, nil
}
