// Package ast provides a high-level data structure for representing Dachs source code.
package ast

import (
	"fmt"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"os"
	"path/filepath"
)

type funcPrinter struct {
	total int
}

func (v *funcPrinter) Visit(n Node) Visitor {
	if f, ok := n.(*Function); ok {
		fmt.Println("Found function", f.Ident.Name)
		v.total++
		return nil
	}
	return v
}

func Example() {
	file := filepath.FromSlash("../syntax/testdata/hello_world.dcs")
	src, err := prelude.NewSourceFromFile(file)
	if err != nil {
		// File not found
		panic(err)
	}

	pos := prelude.Pos{0, 0, 0, src}

	// Abstract syntax tree structure
	root := &Program{
		Toplevels: []Node{
			&Function{
				StartPos: pos,
				EndPos:   pos,
				Ident:    NewSymbol("main"),
				Body: []Statement{
					&RetStmt{StartPos: pos},
				},
			},
		},
	}

	// Create a visitor for AST
	v := &funcPrinter{}

	// Apply visitor
	Visit(v, root)
	// Output: Found function main

	fmt.Println(v.total)
	// Output: 1

	// Print AST structure to stdout
	Fprintln(os.Stdout, root)
	// Output example:
	//   AST of ../syntax/testdata/hello_world.dcs:
	//   Program (toplevels: 1) (0:0-0:0)
	//   -   Function main() (0:0-0:0)
	//   -   -   RetStmt (0:0-0:0)
}
