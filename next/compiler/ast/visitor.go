package ast

// Visitor is an interface for the structs which is used for traversing AST.
type Visitor interface {
	// Visit defines the process when a node is visit.
	// Visitor is a next visitor to use for visit.
	// When wanting to stop visiting, return nil.
	Visit(n Node) Visitor
}

// Visit visits the tree with the visitor.
func Visit(v Visitor, n Node) {
	if n == nil {
		return
	}
	if v = v.Visit(n); v == nil {
		return
	}

	switch n := n.(type) {

	// Toplevel
	case *Program:
		for _, t := range n.Toplevels {
			Visit(v, t)
		}
	case *Typedef:
		Visit(v, n.Type)
	case *EnumTypedef:
		for _, c := range n.Cases {
			Visit(v, c.Child)
		}
	case *Function:
		for _, p := range n.Params {
			Visit(v, p.Type)
		}
		if n.RetType != nil {
			Visit(v, n.RetType)
		}
		for _, s := range n.Body {
			Visit(v, s)
		}

	// Type
	case *TypeInstantiate:
		for _, a := range n.Args {
			Visit(v, a)
		}
	case *RecordType:
		for _, f := range n.Fields {
			Visit(v, f.Type)
		}
	case *TupleType:
		for _, e := range n.Elems {
			Visit(v, e)
		}
	case *FunctionType:
		for _, p := range n.ParamTypes {
			Visit(v, p)
		}
		Visit(v, n.RetType)
	case *TypeofType:
		Visit(v, n.Expr)

	// Pattern
	case *RecordPattern:
		for _, f := range n.Fields {
			Visit(v, f.Pattern)
		}
	case *ArrayPattern:
		for _, e := range n.Elems {
			Visit(v, e)
		}

	// Destructuring
	case *RecordDestructuring:
		for _, f := range n.Fields {
			Visit(v, f.Child)
		}

	// Statement
	case *VarDecl:
		for _, d := range n.Decls {
			Visit(v, d)
		}
		for _, e := range n.RHSExprs {
			Visit(v, e)
		}
	case *VarAssign:
		for _, a := range n.RHSExprs {
			Visit(v, a)
		}
	case *IndexAssign:
		Visit(v, n.Assignee)
		Visit(v, n.Index)
		Visit(v, n.RHS)
	case *RetStmt:
		for _, e := range n.Exprs {
			Visit(v, e)
		}
	case *IfStmt:
		Visit(v, n.Cond)
		for _, s := range n.Then {
			Visit(v, s)
		}
		for _, s := range n.Else {
			Visit(v, s)
		}
	case *SwitchStmt:
		for _, c := range n.Cases {
			Visit(v, c.Cond)
			for _, s := range c.Stmts {
				Visit(v, s)
			}
		}
		for _, s := range n.Else {
			Visit(v, s)
		}
	case *MatchStmt:
		Visit(v, n.Matched)
		for _, c := range n.Cases {
			Visit(v, c.Pattern)
			for _, s := range c.Stmts {
				Visit(v, s)
			}
		}
		for _, s := range n.Else {
			Visit(v, s)
		}
	case *ForEachStmt:
		Visit(v, n.Iterator)
		Visit(v, n.Range)
		for _, s := range n.Body {
			Visit(v, s)
		}
	case *WhileStmt:
		Visit(v, n.Cond)
		for _, s := range n.Body {
			Visit(v, s)
		}
	case *ExprStmt:
		Visit(v, n.Expr)

	// Expressions
	case *ArrayLiteral:
		for _, e := range n.Elems {
			Visit(v, e)
		}
	case *DictLiteral:
		for _, kv := range n.Elems {
			Visit(v, kv.Key)
			Visit(v, kv.Value)
		}
	case *UnaryExpr:
		Visit(v, n.Child)
	case *BinaryExpr:
		Visit(v, n.LHS)
		Visit(v, n.RHS)
	case *SeqExpr:
		for _, s := range n.Stmts {
			Visit(v, s)
		}
		Visit(v, n.LastExpr)
	case *IfExpr:
		Visit(v, n.Cond)
		Visit(v, n.Then)
		Visit(v, n.Else)
	case *SwitchExpr:
		for _, c := range n.Cases {
			Visit(v, c.Cond)
			Visit(v, c.Body)
		}
		Visit(v, n.Else)
	case *MatchExpr:
		Visit(v, n.Matched)
		for _, c := range n.Cases {
			Visit(v, c.Pattern)
			Visit(v, c.Body)
		}
		Visit(v, n.Else)
	case *CoerceExpr:
		Visit(v, n.Expr)
		Visit(v, n.Type)
	case *IndexAccess:
		Visit(v, n.Child)
		Visit(v, n.Index)
	case *FieldAccess:
		Visit(v, n.Child)
	case *RecordLiteral:
		for _, f := range n.Fields {
			Visit(v, f.Expr)
		}
	case *TupleLiteral:
		for _, e := range n.Elems {
			Visit(v, e)
		}
	case *FuncCall:
		Visit(v, n.Callee)
		for _, a := range n.Args {
			Visit(v, a)
		}
		Visit(v, n.DoBlock)
	case *FuncCallNamed:
		Visit(v, n.Callee)
		for _, a := range n.Args {
			Visit(v, a.Expr)
		}
		Visit(v, n.DoBlock)
	case *Lambda:
		for _, p := range n.Params {
			Visit(v, p.Type)
		}
		Visit(v, n.BodyExpr)
	}
}
