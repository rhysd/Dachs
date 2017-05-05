package ast

import (
	"fmt"
	"io"
	"strings"
)

// Printer is a visitor to print AST.
type Printer struct {
	indent int
	out    io.Writer
}

// Visit visits AST node.
func (p Printer) Visit(n Node) Visitor {
	s, e := n.Pos(), n.End()
	fmt.Fprintf(p.out, "\n%s%s (%d:%d-%d:%d)", strings.Repeat("-   ", p.indent), n.String(), s.Line, s.Column, e.Line, e.Column)
	return Printer{p.indent + 1, p.out}
}

// Fprint prints AST nodes to a writer.
func Fprint(out io.Writer, n Node) {
	f := n.Pos().File
	if f != nil {
		fmt.Fprintf(out, "AST of %s:", f.Name)
	} else {
		fmt.Fprint(out, "AST:")
	}
	p := Printer{0, out}
	Visit(p, n)
}

// Fprint prints AST nodes to a writer with newline.
func Fprintln(out io.Writer, n Node) {
	Fprint(out, n)
	fmt.Fprintln(out)
}
