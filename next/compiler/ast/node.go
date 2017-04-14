// Package ast provides a high-level data structure for representing Dachs source code.
package ast

import (
	"fmt"
	"strings"

	"github.com/rhysd/Dachs/next/compiler/prelude"
)

// Symbol represents symbols in source (variable name, type name, ...).
type Symbol struct {
	Name string
	ID   int
}

// NewSymbol creates a new symbol with ID.
func NewSymbol(n string) Symbol {
	return Symbol{
		Name: n,
		ID:   0,
	}
}

// UnaryOp is a kind of unary operator.
type UnaryOp int

const (
	// NegOp represents '-'
	NegOp UnaryOp = iota
	// NotOp represents '!'
	NotOp
)

// BinaryOp is a kind of binary operator.
type BinaryOp int

const (
	// AddOp represents '+'
	AddOp BinaryOp = iota
	// SubOp represents '-'
	SubOp
	// MultOp represents '*'
	MultOp
	// DivOp represents '/'
	DivOp
	// ModOp represents '%'
	ModOp
	// EqOp represents '=='
	EqOp
	// NotEqOp represents '!='
	NotEqOp
	// LessOp represents '<'
	LessOp
	// LessEqOp represents '<='
	LessEqOp
	// GreaterOp represents '>'
	GreaterOp
	// GreaterEqOp represents '>='
	GreaterEqOp
	// AndOp represents '&&'
	AndOp
	// OrOp represents '||'
	OrOp
)

type (
	/*
	 * General
	 */
	Node interface {
		Pos() prelude.Pos
		End() prelude.Pos
		String() string
	}

	// Root
	Program struct {
		Toplevels []Node
	}

	// Toplevels

	// type Name of Type
	Typedef struct {
		StartPos prelude.Pos
		Ident    Symbol
		Type     Type
	}

	FuncParam struct {
		Ident Symbol
		Type  Type
	}

	// func () ... end
	Function struct {
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
		Params   []FuncParam
		Body     []Statement
	}

	// import a.b.c
	//  => Parents: ['a', 'b'] and Imported: []{'c'}
	//
	// import a.b.{c, d, e}
	//  => Parents: ['a', 'b'] and Imported: []{'c', 'd', 'e'}
	//
	// import a.b.*
	//  => Parents: ['a', 'b'] and Imported: []{'*'}
	Import struct {
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Relative bool
		Parents  []string
		Imported []string
	}

	/*
	 * Types
	 */
	Type interface {
		Node
		typeNode()
	}

	// int, float, Foo, ...
	TypeRef struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
	}

	// 'a, 'b, ...
	TypeVar struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
	}

	// T{a, b, ...}
	TypeInstantiate struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
		Args     []Type
	}

	RecordTypeField struct {
		Name string
		Type Type
	}

	// {a, b}
	// {a: int, b: T}
	RecordType struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Fields   []RecordTypeField
		// Note:
		// Should I add Ident to this struct?
		// Currently named record type is invoked with type parameter such as Name{T, U}.
		// If making it possible to invoke T{foo: T, bar: U}, this struct possibly has
		// record name.
	}

	// {_: int, _: str}
	// {_, _}
	TupleType struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Elems    []Type
	}

	// (int, str) -> float
	FunctionType struct {
		Type
		StartPos   prelude.Pos
		ParamTypes []Type
		RetType    Type
	}

	EnumTypeCase struct {
		StartPos prelude.Pos
		Name     string
		Child    *RecordType
	}

	// case Foo
	// case Foo{...}
	EnumType struct {
		Type
		Cases []EnumTypeCase
	}

	// typeof(...)
	TypeofType struct {
		Type
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Expr     Expression
	}

	/*
	 * Pattern
	 */
	Pattern interface {
		Node
		patternNode()
	}

	// 42
	IntConstPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    int64
	}

	// 42u
	UIntConstPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    uint64
	}

	// true
	BoolConstPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    bool
	}

	// "foo"
	StringConstPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    string
	}

	// 3.14
	FloatConstPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    float64
	}

	RecordPatternField struct {
		Name    string
		Pattern Pattern
	}

	// {a, b}
	// {a, b, ...}
	RecordPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
		Fields   []RecordPatternField
	}

	// a
	// _
	VarDeclPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
	}

	// [a, b, c]
	// [a, b, ...]
	ArrayPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Elems    []Pattern
	}

	// '...' of [x, y, ...] or {x, y, ...}
	RestPattern struct {
		Pattern
		StartPos prelude.Pos
		EndPos   prelude.Pos
	}

	/*
	 * Destructuring
	 */
	Destructuring interface {
		Node
		destructuringNode()
	}

	// a
	// _
	VarDeclDestructuring struct {
		Destructuring
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
	}

	// {a, b}
	// {foo: a, bar: b}
	// {a, b, ...}
	// {foo: a, bar: b, ...}
	RecordDestructiring struct {
		Destructuring
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
		Fields   []Destructuring
	}

	// Note:
	// 'Fields' of 'RecordDestructiring' is []Destructuring, not []struct{Name string, Bound Destructuring}.
	// This is because destructuring does not permit {foo: bar} style pattern.
	// If it's permitted, it's complicated to use it with unnamed fields record (tuples).
	// How does `{foo: bar} := {_: 42}` work? It might raise a compilation error. But I don't want
	// to make additional special case related to unnamed fields.
	// So, for now, simply forbid destructuring with different name from field's.

	// '...' of {a, b, ...}
	RestDestructuring struct {
		Destructuring
		StartPos prelude.Pos
		EndPos   prelude.Pos
	}

	/*
	 * Statements
	 */
	Statement interface {
		Node
		statementNode()
	}

	// a := 42
	// var a, b := 42, 42
	// {foo} := {foo: 42}
	// Foo{foo} := Foo{foo: 42}
	// Foo{foo}, {foo} := Foo{foo: 42}, {foo: 42}
	VarDecl struct {
		Statement
		StartPos prelude.Pos
		Mutable  bool
		Decls    []Destructuring
		RHSExprs []Expression
	}

	// a = 42
	// a, b = 42, 42
	VarAssign struct {
		Statement
		StartPos prelude.Pos
		Idents   []Symbol
		RHSExprs []Expression
	}

	// arr[idx] = 42
	IndexAssign struct {
		Statement
		Assignee Expression
		Index    Expression
		RHS      Expression
	}

	// ret expr
	RetStmt struct {
		Statement
		StartPos prelude.Pos
		Exprs    []Expression
	}

	// if true then ... else ... end
	// if false
	//   ...
	// else
	//   ...
	// end
	IfStmt struct {
		Statement
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Cond     Expression
		Then     []Statement
		Else     []Statement
	}

	SwitchStmtCase struct {
		StartPos prelude.Pos
		Cond     Expression
		Stmts    []Statement
	}

	// case foo then ...
	// case bar
	//   ...
	// else
	//   ...
	// end
	SwitchStmt struct {
		Statement
		EndPos prelude.Pos
		Cases  []SwitchStmtCase
		Else   []Statement
	}

	MatchStmtCase struct {
		Pattern Pattern
		Stmts   []Statement
	}

	// match some_expr
	// case foo then ...
	// case {aaa, ...}
	//   ...
	// else
	//   ...
	// end
	MatchStmt struct {
		Statement
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Cases    []MatchStmtCase
		Else     []Statement
	}

	// expr
	ExprStmt struct {
		Statement
		Expr Expression
	}

	/*
	 * Expressions
	 */
	Expression interface {
		Node
		expressionNode()
	}

	// 42
	IntLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    int64
	}

	// 42u
	UIntLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    uint64
	}

	FloatLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    float64
	}

	// true
	BoolLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    bool
	}

	// "..."
	StringLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Value    string
	}

	VarRef struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
	}

	// [e1, e2, ..., en]
	ArrayLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Elems    []Expression
	}

	DictKeyVal struct {
		Key   Expression
		Value Expression
	}

	// [e1 => e2, e3 => e4]
	// [=>]
	DictLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Elems    []DictKeyVal
	}

	// -42
	// !true
	UnaryExpr struct {
		Expression
		StartPos prelude.Pos
		Op       UnaryOp
		Child    Expression
	}

	// 1 + 1
	BinaryExpr struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Op       BinaryOp
		LHS      Expression
		RHS      Expression
	}

	// stmt; stmt; expr
	// stmt
	// stmt
	// expr
	SeqExpr struct {
		Expression
		Stmts    []Statement
		LastExpr Expression
	}

	// if true then 42 else 10 end
	IfExpr struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Cond     Expression
		Then     Expression
		Else     Expression
	}

	SwitchExprCase struct {
		StartPos prelude.Pos
		Cond     Expression
		Body     Expression
	}

	// case foo then 10
	// case piyo then -19
	// end
	SwitchExpr struct {
		Expression
		EndPos prelude.Pos
		Cases  []SwitchExprCase
		Else   Expression
	}

	MatchExprCase struct {
		Pattern Pattern
		Body    Expression
	}

	// match some_expr
	// case {42} then 10
	// case {foo} then foo
	// else 1
	// end
	MatchExpr struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Cases    []MatchExprCase
		Else     Expression
	}

	// expr as type
	CoerceExpr struct {
		Expression
		Expr Expression
		Type Type
	}

	// expr[expr]
	IndexAccess struct {
		Expression
		EndPos prelude.Pos
		Child  Expression
		Index  Expression
	}

	RecordLitField struct {
		Name string
		Expr Expression
	}

	// Foo{name: val, name2: val2}
	RecordLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Ident    Symbol
		Fields   []RecordLitField
	}

	// {_: e1, _: e2, ...}
	TupleLiteral struct {
		Expression
		StartPos prelude.Pos
		EndPos   prelude.Pos
		Elems    []Expression
	}

	// foo(e1, e2, e3)
	// foo()
	FuncCall struct {
		Expression
		EndPos prelude.Pos
		Callee Expression
		Args   []Expression
	}

	NamedArg struct {
		Name string
		Expr Expression
	}

	// foo(a: e1, b: e2)
	FuncCallNamed struct {
		Expression
		EndPos prelude.Pos
		Callee Expression
		Args   []NamedArg
	}
)

// Path builds an import path string
func (i *Import) Path() string {
	head := ""
	if i.Relative {
		head = "."
	}
	tail := i.Imported[0]
	if len(i.Imported) > 1 {
		tail = fmt.Sprintf("{%s}", strings.Join(i.Imported, ","))
	}
	return fmt.Sprintf("%s%s.%s", head, strings.Join(i.Parents, "."), tail)
}

func mayAnonym(name string) string {
	if name == "" {
		return "anonym"
	}
	return name
}

/*
 * Interfaces for Node
 */

func (n *Program) String() string { return fmt.Sprintf("Program (toplevels: %d)", len(n.Toplevels)) }
func (n *Typedef) String() string { return fmt.Sprintf("Typedef (%s)", n.Ident.Name) }
func (n *Function) String() string {
	ns := make([]string, 0, len(n.Params))
	for _, p := range n.Params {
		ns = append(ns, p.Ident.Name)
	}
	return fmt.Sprintf("Function %s(%s)", n.Ident.Name, strings.Join(ns, ","))
}
func (n *Import) String() string  { return fmt.Sprintf("Import (%s)", n.Path()) }
func (n *TypeRef) String() string { return fmt.Sprintf("TypeRef (%s)", n.Ident.Name) }
func (n *TypeVar) String() string { return fmt.Sprintf("TypeVar (%s)", n.Ident.Name) }
func (n *TypeInstantiate) String() string {
	return fmt.Sprintf("TypeInstantiate (%s)", mayAnonym(n.Ident.Name))
}
func (n *RecordType) String() string {
	ns := make([]string, 0, len(n.Fields))
	for _, f := range n.Fields {
		ns = append(ns, f.Name)
	}
	return fmt.Sprintf("RecordType {%s}", strings.Join(ns, ","))
}
func (n *TupleType) String() string    { return "TupleType" }
func (n *FunctionType) String() string { return "FunctionType" }
func (n *EnumType) String() string {
	ns := make([]string, 0, len(n.Cases))
	for _, c := range n.Cases {
		ns = append(ns, c.Name)
	}
	return fmt.Sprintf("EnumType (%s)", strings.Join(ns, ","))
}
func (n *TypeofType) String() string         { return "Typeof" }
func (n *IntConstPattern) String() string    { return fmt.Sprintf("IntConstPattern (%d)", n.Value) }
func (n *UIntConstPattern) String() string   { return fmt.Sprintf("UIntConstPattern (%d)", n.Value) }
func (n *BoolConstPattern) String() string   { return fmt.Sprintf("BoolConstPattern (%v)", n.Value) }
func (n *StringConstPattern) String() string { return fmt.Sprintf("StringConstPattern \"%s\"", n.Value) }
func (n *FloatConstPattern) String() string  { return fmt.Sprintf("FloatConstPattern (%f)", n.Value) }
func (n *RecordPattern) String() string {
	ns := make([]string, 0, len(n.Fields))
	for _, f := range n.Fields {
		ns = append(ns, f.Name)
	}
	return fmt.Sprintf("RecordPattern %s{%s}", mayAnonym(n.Ident.Name), strings.Join(ns, ","))
}
func (n *VarDeclPattern) String() string { return fmt.Sprintf("VarDeclPattern (%s)", n.Ident.Name) }
func (n *ArrayPattern) String() string   { return "ArrayPattern" }
func (n *RestPattern) String() string    { return "RestPattern" }
func (n *VarDeclDestructuring) String() string {
	return fmt.Sprintf("VarDeclDestructuring (%s)", n.Ident.Name)
}
func (n *RecordDestructiring) String() string {
	return fmt.Sprintf("RecordDestructiring (%s)", mayAnonym(n.Ident.Name))
}
func (n *RestDestructuring) String() string { return "RestDestructuring" }
func (n *VarDecl) String() string {
	v := ""
	if n.Mutable {
		v = "var "
	}
	return fmt.Sprintf("VarDecl %s(decls: %d)", v, len(n.Decls))
}
func (n *VarAssign) String() string {
	ns := make([]string, 0, len(n.Idents))
	for _, n := range n.Idents {
		ns = append(ns, n.Name)
	}
	return fmt.Sprintf("VarAssign (%s)", strings.Join(ns, ","))
}
func (n *IndexAssign) String() string   { return "IndexAssign" }
func (n *RetStmt) String() string       { return "RetStmt" }
func (n *IfStmt) String() string        { return "IfStmt" }
func (n *SwitchStmt) String() string    { return "SwitchStmt" }
func (n *MatchStmt) String() string     { return "MatchStmt" }
func (n *ExprStmt) String() string      { return "ExprStmt" }
func (n *IntLiteral) String() string    { return fmt.Sprintf("IntLiteral (%d)", n.Value) }
func (n *UIntLiteral) String() string   { return fmt.Sprintf("UIntLiteral (%d)", n.Value) }
func (n *FloatLiteral) String() string  { return fmt.Sprintf("FloatLiteral (%f)", n.Value) }
func (n *BoolLiteral) String() string   { return fmt.Sprintf("BoolLiteral (%v)", n.Value) }
func (n *StringLiteral) String() string { return fmt.Sprintf("StringLiteral \"%s\"", n.Value) }
func (n *VarRef) String() string        { return fmt.Sprintf("VarRef (%s)", n.Ident.Name) }
func (n *ArrayLiteral) String() string  { return fmt.Sprintf("ArrayLiteral (elems: %d)", len(n.Elems)) }
func (n *DictLiteral) String() string   { return fmt.Sprintf("DictLiteral (elems: %d)", len(n.Elems)) }
func (n *UnaryExpr) String() string     { return "UnaryExpr" }
func (n *BinaryExpr) String() string    { return "BinaryExpr" }
func (n *SeqExpr) String() string       { return "SeqExpr" }
func (n *IfExpr) String() string        { return "IfExpr" }
func (n *SwitchExpr) String() string    { return "SwitchExpr" }
func (n *MatchExpr) String() string     { return "MatchExpr" }
func (n *CoerceExpr) String() string    { return "CoerceExpr" }
func (n *IndexAccess) String() string   { return "IndexAccess" }
func (n *RecordLiteral) String() string {
	ns := make([]string, 0, len(n.Fields))
	for _, f := range n.Fields {
		ns = append(ns, f.Name)
	}
	return fmt.Sprintf("RecordLiteral %s{%s}", mayAnonym(n.Ident.Name), strings.Join(ns, ","))
}
func (n *TupleLiteral) String() string  { return fmt.Sprintf("TupleLiteral (elems: %d)", len(n.Elems)) }
func (n *FuncCall) String() string      { return "FuncCall" }
func (n *FuncCallNamed) String() string { return "FuncCallNamed" }

// Note: len(n.Toplevels) is never zero because main function is required
func (n *Program) Pos() prelude.Pos              { return n.Toplevels[0].Pos() }
func (n *Program) End() prelude.Pos              { return n.Toplevels[len(n.Toplevels)-1].End() }
func (n *Typedef) Pos() prelude.Pos              { return n.StartPos }
func (n *Typedef) End() prelude.Pos              { return n.Type.End() }
func (n *Function) Pos() prelude.Pos             { return n.StartPos }
func (n *Function) End() prelude.Pos             { return n.EndPos }
func (n *Import) Pos() prelude.Pos               { return n.StartPos }
func (n *Import) End() prelude.Pos               { return n.EndPos }
func (n *TypeRef) Pos() prelude.Pos              { return n.StartPos }
func (n *TypeRef) End() prelude.Pos              { return n.EndPos }
func (n *TypeVar) Pos() prelude.Pos              { return n.StartPos }
func (n *TypeVar) End() prelude.Pos              { return n.EndPos }
func (n *TypeInstantiate) Pos() prelude.Pos      { return n.StartPos }
func (n *TypeInstantiate) End() prelude.Pos      { return n.EndPos }
func (n *RecordType) Pos() prelude.Pos           { return n.StartPos }
func (n *RecordType) End() prelude.Pos           { return n.EndPos }
func (n *TupleType) Pos() prelude.Pos            { return n.StartPos }
func (n *TupleType) End() prelude.Pos            { return n.EndPos }
func (n *FunctionType) Pos() prelude.Pos         { return n.StartPos }
func (n *FunctionType) End() prelude.Pos         { return n.RetType.End() }
func (n *EnumType) Pos() prelude.Pos             { return n.Cases[0].StartPos }
func (n *EnumType) End() prelude.Pos             { return n.Cases[len(n.Cases)-1].Child.End() }
func (n *TypeofType) Pos() prelude.Pos           { return n.StartPos }
func (n *TypeofType) End() prelude.Pos           { return n.EndPos }
func (n *IntConstPattern) Pos() prelude.Pos      { return n.StartPos }
func (n *IntConstPattern) End() prelude.Pos      { return n.EndPos }
func (n *UIntConstPattern) Pos() prelude.Pos     { return n.StartPos }
func (n *UIntConstPattern) End() prelude.Pos     { return n.EndPos }
func (n *BoolConstPattern) Pos() prelude.Pos     { return n.StartPos }
func (n *BoolConstPattern) End() prelude.Pos     { return n.EndPos }
func (n *StringConstPattern) Pos() prelude.Pos   { return n.StartPos }
func (n *StringConstPattern) End() prelude.Pos   { return n.EndPos }
func (n *FloatConstPattern) Pos() prelude.Pos    { return n.StartPos }
func (n *FloatConstPattern) End() prelude.Pos    { return n.EndPos }
func (n *RecordPattern) Pos() prelude.Pos        { return n.StartPos }
func (n *RecordPattern) End() prelude.Pos        { return n.EndPos }
func (n *VarDeclPattern) Pos() prelude.Pos       { return n.StartPos }
func (n *VarDeclPattern) End() prelude.Pos       { return n.EndPos }
func (n *ArrayPattern) Pos() prelude.Pos         { return n.StartPos }
func (n *ArrayPattern) End() prelude.Pos         { return n.EndPos }
func (n *RestPattern) Pos() prelude.Pos          { return n.StartPos }
func (n *RestPattern) End() prelude.Pos          { return n.EndPos }
func (n *VarDeclDestructuring) Pos() prelude.Pos { return n.StartPos }
func (n *VarDeclDestructuring) End() prelude.Pos { return n.EndPos }
func (n *RecordDestructiring) Pos() prelude.Pos  { return n.StartPos }
func (n *RecordDestructiring) End() prelude.Pos  { return n.EndPos }
func (n *RestDestructuring) Pos() prelude.Pos    { return n.StartPos }
func (n *RestDestructuring) End() prelude.Pos    { return n.EndPos }
func (n *VarDecl) Pos() prelude.Pos              { return n.StartPos }
func (n *VarDecl) End() prelude.Pos              { return n.RHSExprs[len(n.RHSExprs)-1].End() }
func (n *VarAssign) Pos() prelude.Pos            { return n.StartPos }
func (n *VarAssign) End() prelude.Pos            { return n.RHSExprs[len(n.RHSExprs)-1].End() }
func (n *IndexAssign) Pos() prelude.Pos          { return n.Assignee.Pos() }
func (n *IndexAssign) End() prelude.Pos          { return n.RHS.End() }
func (n *RetStmt) Pos() prelude.Pos              { return n.StartPos }
func (n *RetStmt) End() prelude.Pos {
	len := len(n.Exprs)
	if len == 0 {
		return n.StartPos
	}
	return n.Exprs[len-1].End()
}
func (n *IfStmt) Pos() prelude.Pos        { return n.StartPos }
func (n *IfStmt) End() prelude.Pos        { return n.EndPos }
func (n *SwitchStmt) Pos() prelude.Pos    { return n.Cases[0].StartPos }
func (n *SwitchStmt) End() prelude.Pos    { return n.EndPos }
func (n *MatchStmt) Pos() prelude.Pos     { return n.StartPos }
func (n *MatchStmt) End() prelude.Pos     { return n.EndPos }
func (n *ExprStmt) Pos() prelude.Pos      { return n.Expr.Pos() }
func (n *ExprStmt) End() prelude.Pos      { return n.Expr.End() }
func (n *IntLiteral) Pos() prelude.Pos    { return n.StartPos }
func (n *IntLiteral) End() prelude.Pos    { return n.EndPos }
func (n *UIntLiteral) Pos() prelude.Pos   { return n.StartPos }
func (n *UIntLiteral) End() prelude.Pos   { return n.EndPos }
func (n *FloatLiteral) Pos() prelude.Pos  { return n.StartPos }
func (n *FloatLiteral) End() prelude.Pos  { return n.EndPos }
func (n *BoolLiteral) Pos() prelude.Pos   { return n.StartPos }
func (n *BoolLiteral) End() prelude.Pos   { return n.EndPos }
func (n *StringLiteral) Pos() prelude.Pos { return n.StartPos }
func (n *StringLiteral) End() prelude.Pos { return n.EndPos }
func (n *VarRef) Pos() prelude.Pos        { return n.StartPos }
func (n *VarRef) End() prelude.Pos        { return n.EndPos }
func (n *ArrayLiteral) Pos() prelude.Pos  { return n.StartPos }
func (n *ArrayLiteral) End() prelude.Pos  { return n.EndPos }
func (n *DictLiteral) Pos() prelude.Pos   { return n.StartPos }
func (n *DictLiteral) End() prelude.Pos   { return n.EndPos }
func (n *UnaryExpr) Pos() prelude.Pos     { return n.StartPos }
func (n *UnaryExpr) End() prelude.Pos     { return n.Child.End() }
func (n *BinaryExpr) Pos() prelude.Pos    { return n.StartPos }
func (n *BinaryExpr) End() prelude.Pos    { return n.EndPos }
func (n *SeqExpr) Pos() prelude.Pos {
	if len(n.Stmts) == 0 {
		return n.LastExpr.Pos()
	}
	return n.Stmts[0].Pos()
}
func (n *SeqExpr) End() prelude.Pos       { return n.LastExpr.End() }
func (n *IfExpr) Pos() prelude.Pos        { return n.StartPos }
func (n *IfExpr) End() prelude.Pos        { return n.EndPos }
func (n *SwitchExpr) Pos() prelude.Pos    { return n.Cases[0].StartPos }
func (n *SwitchExpr) End() prelude.Pos    { return n.EndPos }
func (n *MatchExpr) Pos() prelude.Pos     { return n.StartPos }
func (n *MatchExpr) End() prelude.Pos     { return n.EndPos }
func (n *CoerceExpr) Pos() prelude.Pos    { return n.Expr.Pos() }
func (n *CoerceExpr) End() prelude.Pos    { return n.Type.End() }
func (n *IndexAccess) Pos() prelude.Pos   { return n.Child.Pos() }
func (n *IndexAccess) End() prelude.Pos   { return n.EndPos }
func (n *RecordLiteral) Pos() prelude.Pos { return n.StartPos }
func (n *RecordLiteral) End() prelude.Pos { return n.EndPos }
func (n *TupleLiteral) Pos() prelude.Pos  { return n.StartPos }
func (n *TupleLiteral) End() prelude.Pos  { return n.EndPos }
func (n *FuncCall) Pos() prelude.Pos      { return n.Callee.Pos() }
func (n *FuncCall) End() prelude.Pos      { return n.EndPos }
func (n *FuncCallNamed) Pos() prelude.Pos { return n.Callee.Pos() }
func (n *FuncCallNamed) End() prelude.Pos { return n.EndPos }
