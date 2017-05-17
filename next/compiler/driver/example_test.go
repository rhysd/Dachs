// Package driver provides a class to glue all modules of Dachs compiler implementation.
package driver

import (
	"fmt"
	"os"
	"path/filepath"
)

func Example() {
	// Path to file. Empty string means stdin.
	file := filepath.FromSlash("../syntax/testdata/hello_world.dcs")
	// Tag names to enable logs. "All", "Parsing", "Sema" are available.
	logs := []string{}
	driver, err := NewDriver(file, logs)
	if err != nil {
		panic(err)
	}

	// Returns an array of tokens
	tokens, err := driver.Lex()
	if err != nil {
		panic(err)
	}
	fmt.Println(tokens)

	// Parse into AST
	prog, err := driver.Parse()
	if err != nil {
		panic(err)
	}
	fmt.Println(prog)

	// Print structure of AST
	if err := driver.FprintAST(os.Stdout); err != nil {
		panic(err)
	}
}
