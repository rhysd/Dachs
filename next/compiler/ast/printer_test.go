package ast

import (
	"bytes"
	"github.com/rhysd/Dachs/next/compiler/prelude"
	"io"
	"strings"
	"testing"
)

func TestPrint(t *testing.T) {
	var buf bytes.Buffer

	root := &Program{
		Toplevels: []Node{
			&Typedef{
				Name: NewSymbol("e"),
				Type: &TupleType{
					Elems: []Type{
						&TypeInstantiate{
							Name: NewSymbol("f"),
							Args: []Type{
								&TypeRef{Name: NewSymbol("int")},
							},
						},
						&FunctionType{
							ParamTypes: []Type{
								&TypeRef{Name: NewSymbol("int")},
							},
							RetType: &TypeofType{Expr: &IntLiteral{Value: 10}},
						},
						&EnumType{
							Cases: []EnumTypeCase{
								{
									Name: "hey",
									Child: &RecordType{
										Fields: []RecordTypeField{
											RecordTypeField{"b", &TypeVar{Name: NewSymbol("c")}},
										},
									},
								},
							},
						},
					},
				},
			},
			&Function{
				Name: NewSymbol("foo"),
				Params: []FuncParam{
					FuncParam{NewSymbol("a"), &TypeRef{Name: NewSymbol("d")}},
				},
				Body: []Statement{
					&VarDecl{
						Mutable: true,
						Decls: []Destructuring{
							&RecordDestructiring{
								Fields: []Destructuring{
									&VarDeclDestructuring{
										Name: NewSymbol("g"),
									},
									&RestDestructuring{},
								},
							},
						},
						RHSExprs: []Expression{
							&UIntLiteral{
								Value: uint64(42),
							},
						},
					},
					&VarAssign{
						Names: []Symbol{NewSymbol("h")},
						RHSExprs: []Expression{
							&UnaryExpr{
								Child: &BoolLiteral{Value: true},
							},
							&BinaryExpr{
								LHS: &StringLiteral{
									Value: "hello",
								},
								RHS: &FloatLiteral{
									Value: 3.14,
								},
							},
						},
					},
					&IndexAssign{
						Assignee: &ArrayLiteral{},
						Index:    &IntLiteral{Value: 0},
						RHS: &CoerceExpr{
							Expr: &RecordLiteral{
								Name: Symbol{},
								Fields: []RecordLitField{
									{
										Name: "_",
										Expr: &IntLiteral{Value: 42},
									},
								},
							},
							Type: &TypeRef{Name: NewSymbol("i")},
						},
					},
					&IfStmt{
						Cond: &BoolLiteral{Value: false},
						Then: []Statement{
							&ExprStmt{
								Expr: &IfExpr{
									Cond: &BoolLiteral{Value: false},
									Then: &SeqExpr{
										Stmts: []Statement{
											&RetStmt{},
										},
										LastExpr: &IntLiteral{Value: 42},
									},
									Else: &SeqExpr{
										LastExpr: &FloatLiteral{Value: 1.0},
									},
								},
							},
						},
						Else: []Statement{
							&ExprStmt{
								Expr: &FuncCall{
									Callee: &VarRef{Name: NewSymbol("f")},
									Args: []Expression{
										&IntLiteral{Value: 10},
									},
								},
							},
						},
					},
					&SwitchStmt{
						Cases: []SwitchStmtCase{
							{
								Cond: &BoolLiteral{Value: false},
								Stmts: []Statement{
									&ExprStmt{
										Expr: &IntLiteral{Value: 42},
									},
								},
							},
						},
						Else: []Statement{
							&ExprStmt{
								Expr: &SwitchExpr{
									Cases: []SwitchExprCase{
										{
											Cond: &BoolLiteral{Value: true},
											Body: &IntLiteral{Value: 12},
										},
									},
									Else: &FuncCallNamed{
										Callee: &VarRef{Name: NewSymbol("g")},
										Args: []NamedArg{
											{Name: "foo", Expr: &FloatLiteral{Value: 1.0}},
										},
									},
								},
							},
						},
					},
					&MatchStmt{
						Cases: []MatchStmtCase{
							{
								Pattern: &ArrayPattern{
									Elems: []Pattern{
										&BoolConstPattern{
											Value: false,
										},
										&VarDeclPattern{
											Name: NewSymbol("pat"),
										},
									},
								},
								Stmts: []Statement{
									&RetStmt{},
								},
							},
						},
						Else: []Statement{
							&ExprStmt{
								Expr: &MatchExpr{
									Cases: []MatchExprCase{
										{
											Pattern: &RecordPattern{
												Name: NewSymbol("patt"),
												Fields: []RecordPatternField{
													{
														Name:    "a",
														Pattern: &VarDeclPattern{Name: NewSymbol("a")},
													},
													{
														Name:    "b",
														Pattern: &StringConstPattern{Value: "hello"},
													},
													{
														Name:    "c",
														Pattern: &IntConstPattern{Value: 42},
													},
													{
														Name:    "d",
														Pattern: &UIntConstPattern{Value: uint64(11)},
													},
													{
														Name:    "e",
														Pattern: &FloatConstPattern{Value: 2.7},
													},
													{
														Name:    "",
														Pattern: &RestPattern{},
													},
												},
											},
											Body: &IntLiteral{Value: 10},
										},
									},
									Else: &TupleLiteral{
										Elems: []Expression{
											&DictLiteral{
												Elems: []DictKeyVal{
													{
														Key: &StringLiteral{
															Value: "foo",
														},
														Value: &UIntLiteral{
															Value: uint64(3),
														},
													},
												},
											},
										},
									},
								},
							},
						},
					},
					&RetStmt{
						Exprs: []Expression{
							&IntLiteral{
								Value: 42,
							},
							&IndexAccess{
								Child: &ArrayLiteral{
									Elems: []Expression{
										&IntLiteral{Value: 3},
									},
								},
								Index: &IntLiteral{Value: 0},
							},
						},
					},
				},
			},
			&Import{
				Relative: true,
				Parents:  []string{"foo", "bar"},
				Imported: []string{"piyo", "poyo"},
			},
		},
	}

	Fprintln(&buf, root)

	want := strings.Split(`AST:
-   Program
-   -   Typedef (e)
-   -   -   TupleType
-   -   -   -   TypeInstantiate (f)
-   -   -   -   -   TypeRef
-   -   -   -   FunctionType
-   -   -   -   -   TypeRef (int)
-   -   -   -   -   Typeof
-   -   -   -   -   -   IntLiteral (10)
-   -   -   -   EnumType
-   -   -   -   -   RecordType
-   -   -   -   -   -   TypeVar (c)
-   -   Function (foo)
-   -   -   TypeRef (d)
-   -   -   VarDecl var
-   -   -   -   RecordDestructiring
-   -   -   -   -   VarDeclDestructuring (g)
-   -   -   -   -   RestDestructuring
-   -   -   -   UIntLiteral (42)
-   -   -   VarAssign (h)
-   -   -   -   UnaryExpr
-   -   -   -   -   BoolLiteral
-   -   -   -   BinaryExpr
-   -   -   -   -   StringLiteral "hello"
-   -   -   -   -   FloatLiteral (3.14
-   -   -   IndexAssign
-   -   -   -   ArrayLiteral (elems: 0)
-   -   -   -   IntLiteral (0)
-   -   -   -   CoerceExpr
-   -   -   -   -   RecordLiteral (anonym)
-   -   -   -   -   -   IntLiteral (42)
-   -   -   -   -   TypeRef (i)
-   -   -   IfStmt
-   -   -   -   BoolLiteral (false)
-   -   -   -   ExprStmt
-   -   -   -   -   IfExpr
-   -   -   -   -   -   BoolLiteral
-   -   -   -   -   -   SeqExpr
-   -   -   -   -   -   -   RetStmt
-   -   -   -   -   -   -   IntLiteral (42)
-   -   -   -   -   -   SeqExpr
-   -   -   -   -   -   -   FloatLiteral (1.0
-   -   -   -   ExprStmt
-   -   -   -   -   FuncCall
-   -   -   -   -   -   VarRef (f)
-   -   -   -   -   -   IntLiteral (10)
-   -   -   SwitchStmt
-   -   -   -   BoolLiteral (false)
-   -   -   -   ExprStmt
-   -   -   -   -   IntLiteral (42)
-   -   -   -   ExprStmt
-   -   -   -   -   SwitchExpr
-   -   -   -   -   -   BoolLiteral (true)
-   -   -   -   -   -   IntLiteral (12)
-   -   -   -   -   -   FuncCallNamed
-   -   -   -   -   -   -   VarRef (g)
-   -   -   -   -   -   -   FloatLiteral (1.0
-   -   -   MatchStmt
-   -   -   -   ArrayPattern
-   -   -   -   -   BoolConstPattern (false)
-   -   -   -   -   VarDeclPattern (pat)
-   -   -   -   RetStmt
-   -   -   -   ExprStmt
-   -   -   -   -   MatchExpr
-   -   -   -   -   -   RecordPattern
-   -   -   -   -   -   -   VarDeclPattern (a)
-   -   -   -   -   -   -   StringConstPattern "hello"
-   -   -   -   -   -   -   IntConstPattern (42)
-   -   -   -   -   -   -   UIntConstPattern (11)
-   -   -   -   -   -   -   FloatConstPattern (2.7
-   -   -   -   -   -   -   RestPattern
-   -   -   -   -   -   IntLiteral (10)
-   -   -   -   -   -   TupleLiteral (elems: 1)
-   -   -   -   -   -   -   DictLiteral (elems: 1)
-   -   -   -   -   -   -   -   StringLiteral "foo"
-   -   -   -   -   -   -   -   UIntLiteral (3)
-   -   -   RetStmt
-   -   -   -   IntLiteral (42)
-   -   -   -   IndexAccess
-   -   -   -   -   ArrayLiteral (elems: 1)
-   -   -   -   -   -   IntLiteral (3)
-   -   -   -   -   IntLiteral (0)
-   -   Import (.foo.bar.{piyo,poyo})
`, "\n")

	got := make([]string, 0, len(want))
	for {
		s, err := buf.ReadString('\n')
		got = append(got, s)
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
	}

	if len(got) != len(want) {
		t.Fatalf("Expected %d lines as output but actually it was %d lines\nOutput: '%s'", len(want), len(got), strings.Join(got, ""))
	}

	for i, w := range want {
		g := got[i]
		if !strings.HasPrefix(g, w) {
			t.Fatalf("Unexpected output at line %d\n  Want: '%s'\n  Got:  '%s'\nOutput: '%s'", i+1, w, g, strings.Join(got, ""))
		}
	}
}

func TestHeader(t *testing.T) {
	src := prelude.NewDummySource("func main; end")
	pos := prelude.Pos{File: src}
	root := &Program{
		Toplevels: []Node{
			&Function{
				StartPos: pos,
				EndPos:   pos,
				Name:     NewSymbol("main"),
			},
		},
	}

	var buf bytes.Buffer
	Fprintln(&buf, root)
	got, err := buf.ReadString('\n')
	if err != nil {
		t.Fatal(err)
	}
	want := "AST of <dummy>:\n"
	if got != want {
		t.Fatalf("Want '%s' as first line but actually '%s'", want, got)
	}
}
