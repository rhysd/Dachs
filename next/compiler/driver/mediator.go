package driver

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"github.com/rhysd/Dachs/next/compiler/syntax"
	"io"
)

// Driver is a mediator of Dachs compiler as a frontend of LLVM.
type Driver struct {
	Source *prelude.Source
}

func setupLog(logs []string) error {
	for _, tag := range logs {
		switch tag {
		case "All":
			prelude.EnableLog(prelude.All)
		case "Parsing":
			prelude.EnableLog(prelude.Parsing)
		case "Sema":
			prelude.EnableLog(prelude.Sema)
		default:
			return fmt.Errorf("Unknown log component '%s'. Valid component: 'Parsing', 'Sema' or 'All'", tag)
		}
	}
	return nil
}

// NewDriver make a new instance of driver and setup logger for compiler.
// If "" is given as a first argument, driver will attempt to read source code from stdin.
func NewDriver(file string, logs []string) (*Driver, error) {
	var src *prelude.Source
	var err error

	if file == "" {
		src, err = prelude.NewSourceFromStdin()
	} else {
		src, err = prelude.NewSourceFromFile(file)
	}

	if err != nil {
		return nil, fmt.Errorf("Error on opening source: %s", err.Error())
	}

	if err = setupLog(logs); err != nil {
		return nil, err
	}

	return &Driver{Source: src}, nil
}

// Lex only lexes a given source
func (d *Driver) Lex() ([]*syntax.Token, error) {
	prelude.NowLogging(prelude.Parsing)

	var err error
	l := syntax.NewLexer(d.Source)
	l.Error = func(e *prelude.Error) {
		err = e
	}

	tok := []*syntax.Token{}
	go l.Lex()
	for {
		select {
		case t := <-l.Tokens:
			tok = append(tok, t)
			switch t.Kind {
			case syntax.TokenEOF, syntax.TokenIllegal:
				close(l.Tokens)
				return tok, err
			}
		}
	}
}

func (d *Driver) Parse() (*ast.Program, error) {
	prelude.NowLogging(prelude.Parsing)
	return syntax.Parse(d.Source)
}

func (d *Driver) FprintAST(out io.Writer) error {
	root, err := d.Parse()
	if err != nil {
		return err
	}
	ast.Fprintln(out, root)
	return nil
}

func (d *Driver) Analyze() /*TODO: Return analyzation result*/ error {
	root, err := d.Parse()
	if err != nil {
		return err
	}
	if err := syntax.ResolveImports(root); err != nil {
		return err
	}
	panic("Not implemented yet")
	return nil
}
