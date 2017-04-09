// Package driver provides a class to glue all modules of Dachs compiler implementation.
package driver

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"github.com/rhysd/Dachs/next/compiler/syntax"
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
		case "Lexing":
			prelude.EnableLog(prelude.Lexing)
		case "Parsing":
			prelude.EnableLog(prelude.Parsing)
		default:
			return fmt.Errorf("Unknown log component '%s'. Valid component: 'Lexing', 'Parsing' or 'All'", tag)
		}
	}
	return nil
}

// NewDriver make a new instance of driver and setup logger for compiler
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
