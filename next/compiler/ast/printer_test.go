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
				Ident: NewSymbol("e"),
				Type: &TupleType{
					Elems: []Type{
						&TypeInstantiate{
							Ident: NewSymbol("f"),
							Args: []Type{
								&TypeRef{Ident: NewSymbol("int")},
							},
						},
						&FunctionType{
							ParamTypes: []Type{
								&TypeRef{Ident: NewSymbol("int")},
							},
							RetType: &TypeofType{Expr: &IntLiteral{Value: 10}},
						},
						&ArrayType{
							Elem: &TypeRef{Ident: NewSymbol("int")},
						},
						&DictType{
							Key:   &TypeRef{Ident: NewSymbol("string")},
							Value: &TypeRef{Ident: NewSymbol("int")},
						},
					},
				},
			},
			&EnumTypedef{
				Ident: NewSymbol("z"),
				Cases: []EnumTypeCase{
					{
						Name: "hey",
						Child: &RecordType{
							Fields: []RecordTypeField{
								RecordTypeField{"b", &TypeVar{Ident: NewSymbol("c")}},
							},
						},
					},
				},
			},
			&Function{
				Ident: NewSymbol("foo"),
				Params: []FuncParam{
					{NewSymbol("a"), &TypeRef{Ident: NewSymbol("d")}},
				},
				RetType: &TypeRef{Ident: NewSymbol("Foo")},
				Body: []Statement{
					&VarDecl{
						Mutable: true,
						Decls: []Destructuring{
							&RecordDestructuring{
								Fields: []RecordDestructuringField{
									{
										"g",
										&VarDeclDestructuring{
											Ident: NewSymbol("g"),
										},
									},
									{
										"hey",
										&VarDeclDestructuring{
											Ident: NewSymbol("h"),
										},
									},
								},
							},
							&RecordDestructuring{
								Ident:    NewSymbol("Blah"),
								EnumName: "Woo",
								Fields: []RecordDestructuringField{
									{
										"i",
										&VarDeclDestructuring{
											Ident: NewSymbol("i"),
										},
									},
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
						Idents: []Symbol{NewSymbol("h")},
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
								Ident: Symbol{},
								Fields: []RecordLitField{
									{
										Name: "_",
										Expr: &IntLiteral{Value: 42},
									},
								},
							},
							Type: &TypeRef{Ident: NewSymbol("i")},
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
									Callee: &VarRef{Ident: NewSymbol("f")},
									Args: []Expression{
										&IntLiteral{Value: 10},
									},
								},
							},
							&ExprStmt{
								Expr: &FuncCall{
									Callee: &VarRef{Ident: NewSymbol("f")},
									Args: []Expression{
										&IntLiteral{Value: 10},
									},
									DoBlock: &Lambda{
										IsDoBlock: true,
										Params:    []FuncParam{},
										BodyExpr:  &IntLiteral{Value: 88},
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
										Callee: &VarRef{Ident: NewSymbol("g")},
										Args: []NamedArg{
											{Name: "foo", Expr: &FloatLiteral{Value: 1.0}},
										},
									},
								},
							},
						},
					},
					&MatchStmt{
						Matched: &BoolLiteral{Value: false},
						Arms: []MatchStmtArm{
							{
								Pattern: &ArrayPattern{
									Elems: []Pattern{
										&BoolConstPattern{
											Value: false,
										},
										&VarDeclPattern{
											Ident: NewSymbol("pat"),
										},
									},
									Exhaustive: true,
								},
								Stmts: []Statement{
									&RetStmt{},
								},
							},
						},
						Else: []Statement{
							&ExprStmt{
								Expr: &MatchExpr{
									Matched: &BoolLiteral{Value: false},
									Arms: []MatchExprArm{
										{
											Pattern: &RecordPattern{
												Ident: NewSymbol("patt"),
												Fields: []RecordPatternField{
													{
														Name:    "a",
														Pattern: &VarDeclPattern{Ident: NewSymbol("a")},
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
												},
											},
											Body: &IntLiteral{Value: 10},
										},
										{
											Pattern: &RecordPattern{
												Ident:    NewSymbol("Foo"),
												EnumName: "Bar",
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
					&ExprStmt{
						Expr: &Lambda{
							Params: []FuncParam{
								{NewSymbol("x"), &TypeRef{Ident: NewSymbol("uint")}},
							},
							BodyExpr: &VarRef{Ident: NewSymbol("x")},
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
							&FieldAccess{
								Child: &VarRef{Ident: NewSymbol("r")},
								Name:  "f",
							},
						},
					},
					&ForEachStmt{
						Iterator: &VarDeclDestructuring{
							Ident: NewSymbol("p"),
						},
						Range: &ArrayLiteral{
							Elems: []Expression{
								&TupleLiteral{
									Ident:    NewSymbol("Piyo"),
									EnumName: "Fuga",
								},
							},
						},
						Body: []Statement{
							&ExprStmt{
								Expr: &FuncCallNamed{
									Callee: &VarRef{Ident: NewSymbol("g")},
									Args: []NamedArg{
										{Name: "foo", Expr: &FloatLiteral{Value: 1.0}},
									},
									DoBlock: &Lambda{
										IsDoBlock: true,
										Params:    []FuncParam{},
										BodyExpr:  &IntLiteral{Value: 88},
									},
								},
							},
							&NextStmt{},
						},
					},
					&WhileStmt{
						Cond: &BoolLiteral{Value: true},
						Body: []Statement{
							&ExprStmt{
								Expr: &IntLiteral{Value: 54},
							},
							&BreakStmt{},
						},
					},
				},
			},
			&Import{
				Local:    true,
				Parents:  []string{"foo", "bar"},
				Imported: []string{"piyo", "poyo"},
			},
		},
	}

	Fprintln(&buf, root)

	want := strings.Split(`AST:
Program
-   Typedef (e)
-   -   TupleType
-   -   -   TypeInstantiate (f)
-   -   -   -   TypeRef
-   -   -   FunctionType
-   -   -   -   TypeRef (int)
-   -   -   -   Typeof
-   -   -   -   -   IntLiteral (10)
-   -   -   ArrayType
-   -   -   -   TypeRef (int)
-   -   -   DictType
-   -   -   -   TypeRef (string)
-   -   -   -   TypeRef (int)
-   EnumTypedef z(hey)
-   -   RecordType {b}
-   -   -   TypeVar (c)
-   Function foo(a)
-   -   TypeRef (d)
-   -   TypeRef (Foo)
-   -   VarDecl var
-   -   -   RecordDestructuring anonym{g,hey}
-   -   -   -   VarDeclDestructuring (g)
-   -   -   -   VarDeclDestructuring (h)
-   -   -   RecordDestructuring Blah::Woo{i}
-   -   -   -   VarDeclDestructuring (i)
-   -   -   UIntLiteral (42)
-   -   VarAssign (h)
-   -   -   UnaryExpr
-   -   -   -   BoolLiteral
-   -   -   BinaryExpr
-   -   -   -   StringLiteral "hello"
-   -   -   -   FloatLiteral (3.14
-   -   IndexAssign
-   -   -   ArrayLiteral (elems: 0)
-   -   -   IntLiteral (0)
-   -   -   CoerceExpr
-   -   -   -   RecordLiteral anonym{_}
-   -   -   -   -   IntLiteral (42)
-   -   -   -   TypeRef (i)
-   -   IfStmt
-   -   -   BoolLiteral (false)
-   -   -   ExprStmt
-   -   -   -   IfExpr
-   -   -   -   -   BoolLiteral
-   -   -   -   -   SeqExpr
-   -   -   -   -   -   RetStmt
-   -   -   -   -   -   IntLiteral (42)
-   -   -   -   -   SeqExpr
-   -   -   -   -   -   FloatLiteral (1.0
-   -   -   ExprStmt
-   -   -   -   FuncCall
-   -   -   -   -   VarRef (f)
-   -   -   -   -   IntLiteral (10)
-   -   -   ExprStmt
-   -   -   -   FuncCall
-   -   -   -   -   VarRef (f)
-   -   -   -   -   IntLiteral (10)
-   -   -   -   -   Lambda ()
-   -   -   -   -   -   IntLiteral (88)
-   -   SwitchStmt
-   -   -   BoolLiteral (false)
-   -   -   ExprStmt
-   -   -   -   IntLiteral (42)
-   -   -   ExprStmt
-   -   -   -   SwitchExpr
-   -   -   -   -   BoolLiteral (true)
-   -   -   -   -   IntLiteral (12)
-   -   -   -   -   FuncCallNamed
-   -   -   -   -   -   VarRef (g)
-   -   -   -   -   -   FloatLiteral (1.0
-   -   MatchStmt
-   -   -   BoolLiteral (false)
-   -   -   ArrayPattern
-   -   -   -   BoolConstPattern (false)
-   -   -   -   VarDeclPattern (pat)
-   -   -   RetStmt
-   -   -   ExprStmt
-   -   -   -   MatchExpr
-   -   -   -   -   BoolLiteral (false)
-   -   -   -   -   RecordPattern patt{a,b,c,d,e}
-   -   -   -   -   -   VarDeclPattern (a)
-   -   -   -   -   -   StringConstPattern "hello"
-   -   -   -   -   -   IntConstPattern (42)
-   -   -   -   -   -   UIntConstPattern (11)
-   -   -   -   -   -   FloatConstPattern (2.7
-   -   -   -   -   IntLiteral (10)
-   -   -   -   -   RecordPattern Foo::Bar{}
-   -   -   -   -   IntLiteral (10)
-   -   -   -   -   TupleLiteral (elems: 1)
-   -   -   -   -   -   DictLiteral (elems: 1)
-   -   -   -   -   -   -   StringLiteral "foo"
-   -   -   -   -   -   -   UIntLiteral (3)
-   -   ExprStmt
-   -   -   Lambda (x)
-   -   -   -   TypeRef (uint)
-   -   -   -   VarRef (x)
-   -   RetStmt
-   -   -   IntLiteral (42)
-   -   -   IndexAccess
-   -   -   -   ArrayLiteral (elems: 1)
-   -   -   -   -   IntLiteral (3)
-   -   -   -   IntLiteral (0)
-   -   -   FieldAccess (f)
-   -   -   -   VarRef (r)
-   -   ForEachStmt
-   -   -   VarDeclDestructuring (p)
-   -   -   ArrayLiteral (elems: 1)
-   -   -   -   TupleLiteral Piyo::Fuga(elems: 0)
-   -   -   ExprStmt
-   -   -   -   FuncCallNamed
-   -   -   -   -   VarRef (g)
-   -   -   -   -   FloatLiteral (1.0
-   -   -   -   -   Lambda ()
-   -   -   -   -   -   IntLiteral (88)
-   -   -   NextStmt
-   -   WhileStmt
-   -   -   BoolLiteral (true)
-   -   -   ExprStmt
-   -   -   -   IntLiteral (54)
-   -   -   BreakStmt
-   Import (.foo.bar.{piyo,poyo})
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
				Ident:    NewSymbol("main"),
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
