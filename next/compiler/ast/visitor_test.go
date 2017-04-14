package ast

import (
	"testing"
)

type testRetGatherer struct {
	rets []*RetStmt
}

func (g *testRetGatherer) Visit(n Node) Visitor {
	if ret, ok := n.(*RetStmt); ok {
		g.rets = append(g.rets, ret)
	}
	return g
}

type testFirstBin struct {
	bin *BinaryExpr
}

func (v *testFirstBin) Visit(n Node) Visitor {
	if bin, ok := n.(*BinaryExpr); ok {
		v.bin = bin
		return nil
	}
	return v
}

func TestGatherSpecificNode(t *testing.T) {
	root := &Program{
		Toplevels: []Node{
			&Function{
				Ident:  NewSymbol("foo"),
				Params: []FuncParam{},
				Body: []Statement{
					&RetStmt{
						Exprs: []Expression{
							&IntLiteral{
								Value: 42,
							},
						},
					},
				},
			},
		},
	}

	v := &testRetGatherer{}
	Visit(v, root)

	if len(v.rets) != 1 {
		t.Fatalf("Number of return statements found was unexpected: %d", len(v.rets))
	}
}

func TestCancelVisit(t *testing.T) {
	root := &Program{
		Toplevels: []Node{
			&Function{
				Ident:  NewSymbol("foo"),
				Params: []FuncParam{},
				Body: []Statement{
					&ExprStmt{
						Expr: &BinaryExpr{
							Op:  SubOp,
							LHS: &IntLiteral{Value: 42},
							RHS: &BinaryExpr{
								Op:  MultOp,
								LHS: &IntLiteral{Value: 21},
								RHS: &IntLiteral{Value: 63},
							},
						},
					},
				},
			},
		},
	}

	v := &testFirstBin{}
	Visit(v, root)

	if v.bin == nil {
		t.Fatal("Visitor did not visit any int literal")
	}

	if v.bin.Op != SubOp {
		t.Fatalf("Second int was visit: %d", v.bin.Op)
	}
}
