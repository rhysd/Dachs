// Package syntax provides conversion from source text into abstract syntax tree for Dachs.
package syntax

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
)

func Example() {
	file := filepath.FromSlash("./testdata/hello_world.dcs")
	src, err := prelude.NewSourceFromFile(file)
	if err != nil {
		// File not found
		panic(err)
	}

	// Lex tokens from source text
	lexer := NewLexer(src)

	// Set Error callback. It is called when parse error detected
	lexer.Error = func(e *prelude.Error) {
		fmt.Fprintln(os.Stderr, e)
	}

	// Lexing can be started in another goroutine.
	go lexer.Lex()

	// Receives tokens from channel and parse them into a syntax tree
	tree, err := ParseTokens(lexer.Tokens)
	if err != nil {
		panic(err)
	}

	// Note: Or you can use syntax.Parse() shortcut

	// Parse was successfully done! Use the result
	ast.Fprintln(os.Stdout, tree)
}
