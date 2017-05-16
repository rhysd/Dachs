package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"strings"
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
	e, err := ifExpr(s)
	if err != nil {
		t.Fatal(err)
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

func TestPromoteSwitchOK(t *testing.T) {
	true_ := &ast.BoolLiteral{Value: true}
	last := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}

	stmt := &ast.SwitchStmt{
		Cases: []ast.SwitchStmtCase{
			{true_, []ast.Statement{&ast.VarAssign{}, last}},
			{true_, []ast.Statement{last}},
		},
		Else: []ast.Statement{last},
	}

	expr, err := switchExpr(stmt)
	if err != nil {
		t.Fatal(err)
	}
	c1, ok := expr.Cases[0].Body.(*ast.SeqExpr)
	if !ok {
		t.Fatalf("Clause which contains one or more statements was not converted into SeqExpr: %s", expr.Cases[0].Body.String())
	}
	if len(c1.Stmts) == 0 {
		t.Fatalf("Statements were not passed to StmtExpr")
	}
	if c1.LastExpr != last.Expr {
		t.Fatalf("Wanted %s but got %s as last expression of case clause", last.Expr.String(), c1.LastExpr.String())
	}
	if expr.Cases[1].Body != last.Expr {
		t.Fatalf("Wanted %s but got %s as expression of expression-only case clause", last.Expr.String(), expr.Cases[1].Body.String())
	}
	if expr.Else != last.Expr {
		t.Fatalf("Wanted %s but got %s as expression of expression-only case clause", last.Expr.String(), expr.Else.String())
	}
}

func TestPromoteSwitchError(t *testing.T) {
	start, end := getDummyPos()
	true_ := &ast.BoolLiteral{Value: true}
	i := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}

	stmt := &ast.SwitchStmt{
		StartPos: start,
		EndPos:   end,
		Cases: []ast.SwitchStmtCase{
			{true_, []ast.Statement{}},
		},
		Else: []ast.Statement{},
	}

	var err error
	stmt.Else = append(stmt.Else, i)
	_, err = switchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in case clause")
	}
	if !strings.Contains(err.Error(), "'case' block") {
		t.Error("Unknown error on case clause:", err)
	}
	stmt.Else = []ast.Statement{}
	stmt.Cases[0].Stmts = append(stmt.Cases[0].Stmts, i)
	_, err = switchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in else clause")
	}
	if !strings.Contains(err.Error(), "'else' block") {
		t.Error("Unknown error on else clause:", err)
	}
	stmt.Else = append(stmt.Else, &ast.VarAssign{
		StartPos: start,
		Idents:   []ast.Symbol{ast.NewSymbol("foo")},
		RHSExprs: []ast.Expression{
			&ast.IntLiteral{Value: 42, StartPos: start, EndPos: end},
		},
	})
	_, err = switchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in else clause")
	}
	if !strings.Contains(err.Error(), "'else' block") {
		t.Error("Unknown error on else clause:", err)
	}
}

func TestPromoteMatchOK(t *testing.T) {
	pat := &ast.IntConstPattern{Value: 42}
	last := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}

	stmt := &ast.MatchStmt{
		Arms: []ast.MatchStmtArm{
			{pat, []ast.Statement{&ast.VarAssign{}, last}},
			{pat, []ast.Statement{last}},
		},
		Else: []ast.Statement{last},
	}

	expr, err := matchExpr(stmt)
	if err != nil {
		t.Fatal(err)
	}
	c1, ok := expr.Arms[0].Body.(*ast.SeqExpr)
	if !ok {
		t.Fatalf("Clause which contains one or more statements was not converted into SeqExpr: %s", expr.Arms[0].Body.String())
	}
	if len(c1.Stmts) == 0 {
		t.Fatalf("Statements were not passed to StmtExpr")
	}
	if c1.LastExpr != last.Expr {
		t.Fatalf("Wanted %s but got %s as last expression of 'with' clause", last.Expr.String(), c1.LastExpr.String())
	}
	if expr.Arms[1].Body != last.Expr {
		t.Fatalf("Wanted %s but got %s as expression of expression-only 'with' clause", last.Expr.String(), expr.Arms[1].Body.String())
	}
	if expr.Else != last.Expr {
		t.Fatalf("Wanted %s but got %s as expression of expression-only 'with' clause", last.Expr.String(), expr.Else.String())
	}
}

func TestPromoteMatchError(t *testing.T) {
	start, end := getDummyPos()
	pat := &ast.IntConstPattern{Value: 42}
	i := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}

	stmt := &ast.MatchStmt{
		StartPos: start,
		EndPos:   end,
		Arms: []ast.MatchStmtArm{
			{pat, []ast.Statement{}},
		},
		Else: []ast.Statement{},
	}

	var err error
	stmt.Else = append(stmt.Else, i)
	_, err = matchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in 'with' clause")
	}
	if !strings.Contains(err.Error(), "'with' block") {
		t.Error("Unknown error on case clause:", err)
	}
	stmt.Else = []ast.Statement{}
	stmt.Arms[0].Stmts = append(stmt.Arms[0].Stmts, i)
	_, err = matchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in else clause")
	}
	if !strings.Contains(err.Error(), "'else' block") {
		t.Error("Unknown error on else clause:", err)
	}
	stmt.Else = append(stmt.Else, &ast.VarAssign{
		StartPos: start,
		Idents:   []ast.Symbol{ast.NewSymbol("foo")},
		RHSExprs: []ast.Expression{
			&ast.IntLiteral{Value: 42, StartPos: start, EndPos: end},
		},
	})
	_, err = matchExpr(stmt)
	if err == nil {
		t.Fatalf("Error did not occur for empty block in else clause")
	}
	if !strings.Contains(err.Error(), "'else' block") {
		t.Error("Unknown error on else clause:", err)
	}
}

func TestNestedExprBlock(t *testing.T) {
	true_ := &ast.BoolLiteral{Value: true}
	pat := &ast.IntConstPattern{Value: 42}
	last := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}

	stmt := &ast.SwitchStmt{
		Cases: []ast.SwitchStmtCase{
			{true_, []ast.Statement{
				&ast.VarAssign{},
				&ast.IfStmt{
					Cond: true_,
					Then: []ast.Statement{
						&ast.VarAssign{},
						last,
					},
					Else: []ast.Statement{last},
				},
			}},
		},
		Else: []ast.Statement{
			&ast.MatchStmt{
				Arms: []ast.MatchStmtArm{
					{pat, []ast.Statement{
						&ast.VarAssign{},
						last,
					}},
				},
				Else: []ast.Statement{last},
			},
		},
	}

	expr, err := switchExpr(stmt)
	if err != nil {
		t.Fatal(err)
	}
	c, ok := expr.Cases[0].Body.(*ast.SeqExpr)
	if !ok {
		t.Fatal("'case' clause was not converted into SeqExpr", expr.Cases[0].Body)
	}
	if len(c.Stmts) != 1 {
		t.Fatalf("Number of statements of case clause is wrong: %d", len(c.Stmts))
	}
	if_, ok := c.LastExpr.(*ast.IfExpr)
	if !ok {
		t.Fatal("'if' statement in 'case' clause was not converted into IfExpr", c.LastExpr)
	}
	thenE, ok := if_.Then.(*ast.SeqExpr)
	if !ok {
		t.Fatal("'then' clause was not converted into SeqExpr", thenE)
	}
	if len(thenE.Stmts) != 1 {
		t.Fatalf("Number of statements of 'then' clause is wrong: %d", len(thenE.Stmts))
	}
	if thenE.LastExpr != last.Expr {
		t.Fatal("Last expression of 'then' clause is unexpected", thenE.LastExpr)
	}
	if if_.Else != last.Expr {
		t.Fatal("'else' clause of 'if' was converted into unexpected expression", if_.Else)
	}
	match, ok := expr.Else.(*ast.MatchExpr)
	if !ok {
		t.Fatal("'else' clause of 'switch' was not converted into MatchExpr", expr.Else)
	}
	with, ok := match.Arms[0].Body.(*ast.SeqExpr)
	if !ok {
		t.Fatal("Body of 'with' clause was not converted into SeqExpr", with)
	}
	if len(with.Stmts) != 1 {
		t.Fatalf("Number of statements of 'with' clause is wrong: %d", len(with.Stmts))
	}
	if with.LastExpr != last.Expr {
		t.Fatal("Last expression of 'with' clause is unexpected", with.LastExpr)
	}
	if match.Else != last.Expr {
		t.Fatal("'else' clause of 'match' was converted into unexpected expression", match.Else)
	}
}

func TestDeeplyNestedExprBlock(t *testing.T) {
	true_ := &ast.BoolLiteral{Value: true}
	last := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}
	int_ := &ast.ExprStmt{Expr: &ast.IntLiteral{Value: 42}}
	stmt := &ast.IfStmt{
		Cond: true_,
		Then: []ast.Statement{
			&ast.IfStmt{
				Cond: true_,
				Then: []ast.Statement{
					&ast.IfStmt{
						Cond: true_,
						Then: []ast.Statement{
							int_,
						},
						Else: []ast.Statement{last},
					},
				},
				Else: []ast.Statement{last},
			},
		},
		Else: []ast.Statement{last},
	}
	expr, err := ifExpr(stmt)
	if err != nil {
		t.Fatal(err)
	}
	target := expr.Then.(*ast.IfExpr).Then.(*ast.IfExpr).Then
	if target != int_.Expr {
		t.Fatal("Expression at deepest clause was converted into unexpected expression block", target)
	}
}
