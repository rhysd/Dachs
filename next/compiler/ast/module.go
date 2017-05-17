package ast

type Module struct {
	NameSpace string
	Imported  []string
	ExposeAll bool
	AST       *Program
}
