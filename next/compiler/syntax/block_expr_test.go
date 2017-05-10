package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"testing"
)

func getDummyPos() (prelude.Pos, prelude.Pos) {
	src := prelude.NewDummySource("hello")
	return prelude.Pos{0, 1, 1, src}, prelude.Pos{3, 1, 4, src}
}

func TestPromoteIfOK(t *testing.T) {
	i := &ast.IntLiteral{Value: 42}
	s := &ast.IfStmt{
		Cond: &ast.BoolLiteral{Value: false},
		Then: []ast.Statement{
			&ast.VarAssign{},
			&ast.ExprStmt{
				Expr: i,
			},
		},
		Else: []ast.Statement{
			&ast.ExprStmt{
				Expr: i,
			},
		},
	}
	conv, err := ifExpr(s)
	if err != nil {
		t.Fatal(err)
	}
	e, ok := conv.(*ast.IfExpr)
	if !ok {
		t.Fatalf("Converted expression is not if: %s", conv.String())
	}
	e1, ok := e.Then.(*ast.SeqExpr)
	if !ok {
		t.Fatalf("Then clause of if expression is not SeqExpr: " + e.Then.String())
	}
	if len(e1.Stmts) == 0 {
		t.Fatalf("Then clause must contain one statement but no statement")
	}
	if e1.LastExpr != i {
		t.Fatalf("Last expression of then clause must be an integer 42 but actually %s", e1.LastExpr.String())
	}
	if e.Else != i {
		t.Fatalf("Else clause must be an integer 42 but actually %s", e.Else.String())
	}
}

func TestPromoteIfError(t *testing.T) {
	start, end := getDummyPos()
	s := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}
	stmt := &ast.IfStmt{
		StartPos: start,
		EndPos:   end,
		Cond:     &ast.BoolLiteral{Value: false},
		Then:     []ast.Statement{},
		Else:     []ast.Statement{},
	}

	var err error
	stmt.Then = append(stmt.Then, s)
	_, err = ifExpr(stmt)
	if err == nil {
		t.Errorf("Error should occur when else clause is empty in if expression")
	}
	stmt.Then = []ast.Statement{}

	stmt.Else = append(stmt.Else, s)
	_, err = ifExpr(stmt)
	if err == nil {
		t.Errorf("Error should occur when then clause is empty in if expression")
	}

	stmt.Then = append(stmt.Then, &ast.VarAssign{
		StartPos: start,
		Idents:   []ast.Symbol{ast.NewSymbol("foo")},
		RHSExprs: []ast.Expression{
			&ast.IntLiteral{Value: 42, StartPos: start, EndPos: end},
		},
	})
	_, err = ifExpr(stmt)
	if err == nil {
		t.Errorf("Erro should occur when clauses of if expression cannot be converted to expression")
	}
}
