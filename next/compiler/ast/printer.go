package ast

import (
	"fmt"
	"io"
	"strings"
)

type Printer struct {
	indent int
	out    io.Writer
}

func (p Printer) Visit(n Node) Visitor {
	s, e := n.Pos(), n.End()
	fmt.Fprintf(p.out, "\n%s%s (%d:%d-%d:%d)", strings.Repeat("-   ", p.indent), n.String(), s.Line, s.Column, e.Line, e.Column)
	return Printer{p.indent + 1, p.out}
}

func Fprint(out io.Writer, n Node) {
	p := Printer{1, out}
	Visit(p, n)
}

func Fprintln(out io.Writer, n Node) {
	Fprint(out, n)
	fmt.Fprintln(out)
}
