%{

package syntax

import (
	"fmt"
	"strconv"
	"strings"
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)
%}

%union{
	token *Token
	node ast.Node
	nodes []ast.Node
	stmt ast.Statement
	if_stmt *ast.IfStmt
	switch_stmt *ast.SwitchStmt
	match_stmt *ast.MatchStmt
	stmts []ast.Statement
	type_node ast.Type
	type_nodes []ast.Type
	import_node *ast.Import
	names []string
	params []ast.FuncParam
	var_assign_stmt *ast.VarAssign
	type_fields []ast.RecordTypeField
	pattern ast.Pattern
	patterns []ast.Pattern
	destructuring ast.Destructuring
	destructurings []ast.Destructuring
	rec_destruct_fields []ast.RecordDestructuringField
	rec_destruct_field ast.RecordDestructuringField
	record_pattern *ast.RecordPattern
	rec_pat_fields []ast.RecordPatternField
	rec_pat_field ast.RecordPatternField
	expr ast.Expression
	exprs []ast.Expression
	record_lit_fields []ast.RecordLitField
	record_lit_field ast.RecordLitField
	dict_keyvals []ast.DictKeyVal
	named_args []ast.NamedArg
	lambda *ast.Lambda
	enum_cases []ast.EnumTypeCase
	bool bool
}

%token<token>
	ILLEGAL
	COMMENT
	NEWLINE
	SEMICOLON
	LPAREN
	RPAREN
	LBRACE
	RBRACE
	LBRACKET
	RBRACKET
	IDENT
	BOOL
	INT
	FLOAT
	STRING
	MINUS
	PLUS
	STAR
	DIV
	NOT
	OR
	AND
	PERCENT
	EQUAL
	NOTEQUAL
	ASSIGN
	LESS
	LESSEQUAL
	GREATER
	GREATEREQUAL
	END
	IF
	THEN
	ELSE
	CASE
	MATCH
	RET
	IMPORT
	DOT
	TYPE
	COLON
	FOR
	IN
	FATRIGHTARROW
	COLONCOLON
	TYPEOF
	AS
	FUNC
	DO
	RIGHTARROW
	COMMA
	BREAK
	NEXT
	ELLIPSIS
	SINGLEQUOTE
	VAR
	LET
	SWITCH
	WITH
	EOF

%nonassoc prec_lambda
%left OR
%left AND
%left EQUAL NOTEQUAL
%left LESS GREATER LESSEQUAL GREATEREQUAL
%left PLUS MINUS
%left STAR DIV PERCENT
%left AS
%right NOT
%right prec_unary

%type<token> seps sep newlines elem_sep then mutability
%type<> opt_newlines
%type<import_node> import_body import_spec import_end
%type<node> toplevel import func_def typedef
%type<nodes> toplevels
%type<names> sep_names
%type<enum_cases> enum_typedef_cases
%type<type_node> opt_type_annotate
%type<stmts> block opt_block_sep block_sep
%type<stmt>
	statement_sep
	ret_statement
	var_decl_statement
	expr_statement
	for_statement
	while_statement
	index_assign_statement
	assign_statement
%type<var_assign_stmt> var_assign_lhs
%type<if_stmt> if_statement
%type<switch_stmt> switch_statement
%type<match_stmt> match_statement
%type<switch_stmt> switch_stmt_cases
%type<match_stmt> match_stmt_arms
%type<expr>
	expression
	constant
	postfix_expr
	primary_expr
	record_or_tuple_literal
	record_or_tuple_anonym
	var_ref
	array_literal
	dict_literal
	lambda_expr
%type<exprs> ret_body comma_sep_exprs array_elems func_call_args
%type<record_lit_fields> record_literal_fields
%type<record_lit_field> record_literal_field
%type<params> func_params lambda_params_in lambda_params_do
%type<type_node>
	type
	type_ref
	type_var
	type_instantiate
	type_record_or_tuple
	type_func type_typeof
%type<type_fields> type_record_fields
%type<type_nodes> opt_types types
%type<destructuring> destructuring var_destruct record_destruct
%type<destructurings> destructurings
%type<rec_destruct_fields> record_destruct_fields
%type<rec_destruct_field> record_destruct_field
%type<pattern>
	pattern
	const_pattern
	var_pattern
	record_pattern
	array_pattern
%type<patterns> array_pat_elems
%type<bool> opt_exhaustive_pattern
%type<record_pattern> rec_pat_anonym
%type<rec_pat_field> rec_pat_field
%type<rec_pat_fields> rec_pat_fields
%type<dict_keyvals> dict_elems
%type<named_args> func_call_named_args
%type<lambda> opt_do_end_block
%type<> program

/* XXX */
%type<> hack_record_lbrace

%start program

%%

program:
	opt_newlines toplevels opt_newlines
		{
			yylex.(*pseudoLexer).result = &ast.Program{Toplevels: $2}
		}

toplevels:
	toplevel
		{
			$$ = []ast.Node{$1}
		}
	| toplevels seps toplevel
		{
			$$ = append($1, $3)
		}

toplevel: import | func_def | typedef

import:
	IMPORT opt_newlines import_body
		{
			n := $3
			n.StartPos = $1.Start
			$$ = n
		}

import_body:
	DOT import_spec
		{
			n := $2
			n.Relative = true
			$$ = n
		}
	| import_spec

import_spec:
	IDENT DOT opt_newlines import_spec
		{
			n := $4
			n.Parents = append(n.Parents, $1.Value())
			$$ = n
		}
	| import_end

import_end:
	LBRACE opt_newlines sep_names opt_newlines RBRACE
		{
			$$ = &ast.Import{
				EndPos: $5.End,
				Imported: $3,
			}
		}
	| STAR
		{
			$$ = &ast.Import{
				EndPos: $1.End,
				Imported: []string{"*"},
			}
		}
	| IDENT
		{
			$$ = &ast.Import{
				EndPos: $1.End,
				Imported: []string{$1.Value()},
			}
		}

func_def:
	FUNC IDENT opt_type_annotate sep block END
		{
			$$ = &ast.Function{
				StartPos: $1.Start,
				EndPos: $6.End,
				Ident: ast.NewSymbol($2.Value()),
				RetType: $3,
				Params: []ast.FuncParam{},
				Body: $5,
			}
		}
	| FUNC IDENT LPAREN opt_newlines RPAREN opt_type_annotate sep block END
		{
			$$ = &ast.Function{
				StartPos: $1.Start,
				EndPos: $9.End,
				Ident: ast.NewSymbol($2.Value()),
				RetType: $6,
				Params: []ast.FuncParam{},
				Body: $8,
			}
		}
	| FUNC IDENT LPAREN opt_newlines func_params opt_newlines RPAREN opt_type_annotate sep block END
		{
			$$ = &ast.Function{
				StartPos: $1.Start,
				EndPos: $11.End,
				Ident: ast.NewSymbol($2.Value()),
				RetType: $8,
				Params: $5,
				Body: $10,
			}
		}

func_params:
	IDENT opt_type_annotate
		{
			$$ = []ast.FuncParam{{ast.NewSymbol($1.Value()), $2}}
		}
	| func_params elem_sep IDENT opt_type_annotate
		{
			$$ = append($1, ast.FuncParam{ast.NewSymbol($3.Value()), $4})
		}

typedef:
	TYPE IDENT ASSIGN opt_newlines type
		{
			$$ = &ast.Typedef {
				StartPos: $1.Start,
				Ident: ast.NewSymbol($2.Value()),
				Type: $5,
			}
		}
	| TYPE IDENT ASSIGN opt_newlines enum_typedef_cases
		{
			$$ = &ast.EnumTypedef {
				StartPos: $1.Start,
				Ident: ast.NewSymbol($2.Value()),
				Cases: $5,
			}
		}

enum_typedef_cases:
	CASE IDENT type_record_or_tuple
		{
			$$ = []ast.EnumTypeCase{
				{$2.Value(), $3},
			}
		}
	| enum_typedef_cases newlines CASE IDENT type_record_or_tuple
		{
			$$ = append($1, ast.EnumTypeCase{$4.Value(), $5})
		}

opt_type_annotate: { $$ = nil }
	| COLON type { $$ = $2 }

type: type_var | type_record_or_tuple | type_func | type_instantiate | type_ref | type_typeof

type_var:
	SINGLEQUOTE IDENT
		{
			$$ = &ast.TypeVar{
				StartPos: $1.Start,
				EndPos: $2.End,
				Ident: ast.NewSymbol($1.Value()),
			}
		}

type_record_or_tuple:
	LBRACE opt_newlines type_record_fields opt_newlines RBRACE
		{
			fields := $3
			if len(fields) > 0 && fields[0].Name != "_" {
				for _,f := range fields[1:] {
					if f.Name == "_" {
						yylex.Error("Mixing unnamed and named fields are not permitted at " + $1.String())
					}
				}
				$$ = &ast.RecordType{
					StartPos: $1.Start,
					EndPos: $5.End,
					Fields: fields,
				}
			} else {
				elems := make([]ast.Type, 0, len(fields))
				for _,f := range fields {
					if f.Name != "_" {
						yylex.Error("Mixing unnamed and named fields is not permitted at " + $1.String())
					}
					elems = append(elems, f.Type)
				}
				$$ = &ast.TupleType{
					StartPos: $1.Start,
					EndPos: $5.End,
					Elems: elems,
				}
			}
		}
	| LBRACE opt_newlines RBRACE
		{
			$$ = &ast.TupleType{
				StartPos: $1.Start,
				EndPos: $3.End,
			}
		}

type_record_fields:
	IDENT opt_type_annotate
		{
			$$ = []ast.RecordTypeField{
				{$1.Value(), $2},
			}
		}
	| type_record_fields elem_sep IDENT opt_type_annotate
		{
			$$ = append($1, ast.RecordTypeField{$3.Value(), $4})
		}

type_func:
	LPAREN opt_newlines opt_types opt_newlines RPAREN opt_newlines RIGHTARROW opt_newlines type
		{
			$$ = &ast.FunctionType{
				StartPos: $1.Start,
				ParamTypes: $3,
				RetType: $9,
			}
		}

type_typeof:
	TYPEOF LPAREN opt_newlines expression opt_newlines RPAREN
		{
			$$ = &ast.TypeofType{
				StartPos: $1.Start,
				EndPos: $6.End,
				Expr: $4,
			}
		}

opt_types:
		{
			$$ = []ast.Type{}
		}
	| opt_types elem_sep type
		{
			$$ = append($1, $3)
		}

types:
	 type
		{
			$$ = []ast.Type{$1}
		}
	| types elem_sep type
		{
			$$ = append($1, $3)
		}

type_instantiate:
	IDENT LBRACE opt_newlines types opt_newlines RBRACE
		{
			$$ = &ast.TypeInstantiate{
				StartPos: $1.Start,
				EndPos: $6.End,
				Ident: ast.NewSymbol($1.Value()),
				Args: $4,
			}
		}

type_ref:
	IDENT
		{
			t := $1
			$$ = &ast.TypeRef{
				StartPos: t.Start,
				EndPos: t.End,
				Ident: ast.NewSymbol(t.Value()),
			}
		}

/* block maybe ends with separator */
opt_block_sep:
		{
			$$ = []ast.Statement{}
		}
	| opt_block_sep statement_sep
		{
			s := $2
			if s != nil {
				$$ = append($1, s)
			} else {
				$$ = $1
			}
		}

/* block ends with separator */
block_sep:
	statement_sep
		{
			s := $1
			if s == nil {
				$$ = []ast.Statement{}
			} else {
				$$ = []ast.Statement{s}
			}
		}
	| block_sep statement_sep
		{
			s := $2
			if s != nil {
				$$ = append($1, s)
			} else {
				$$ = $1
			}
		}

/*
  Omitting the last separater at below statements is not permitted.
    - if_statement
    - switch_statement
    - match_statement

  This is because the statements may be also expressions.
  If permitting to omit trailing separater when one of the statements are the last one of the block,
  it causes a conflict with if_statement/switch_statement/match_statement rules in expression rule.

  Otherwise the last separator can be omitted. It's useful when writing a statement in one line.
    e.g.
      if cond then a = 42 else b = 12 end
*/
block:
	opt_block_sep { $$ = $1 }
	| opt_block_sep ret_statement { $$ = append($1, $2) }
	| opt_block_sep for_statement { $$ = append($1, $2) }
	| opt_block_sep while_statement { $$ = append($1, $2) }
	| opt_block_sep index_assign_statement { $$ = append($1, $2) }
	| opt_block_sep assign_statement { $$ = append($1, $2) }
	| opt_block_sep var_decl_statement { $$ = append($1, $2) }
	| opt_block_sep expr_statement { $$ = append($1, $2) }

statement_sep:
	ret_statement sep { $$ = $1 }
	| if_statement sep { $$ = $1 }
	| switch_statement sep { $$ = $1 }
	| match_statement sep { $$ = $1 }
	| for_statement sep { $$ = $1 }
	| while_statement sep { $$ = $1 }
	| index_assign_statement sep { $$ = $1 }
	| assign_statement sep { $$ = $1 }
	| var_decl_statement sep { $$ = $1 }
	| expr_statement sep { $$ = $1 }
	| sep { $$ = nil }

ret_statement:
	RET ret_body
		{
			$$ = &ast.RetStmt{
				StartPos: $1.Start,
				Exprs: $2,
			}
		}
	| RET sep
		{
			$$ = &ast.RetStmt{
				StartPos: $1.Start,
			}
		}

ret_body:
	expression
		{
			$$ = []ast.Expression{$1}
		}
	| ret_body COMMA opt_newlines expression
		{
			$$ = append($1, $4)
		}

if_statement:
	IF expression then block END
		{
			$$ = &ast.IfStmt{
				StartPos: $1.Start,
				EndPos: $5.End,
				Cond: $2,
				Then: $4,
			}
		}
	| IF expression then block ELSE block END
		{
			$$ = &ast.IfStmt{
				StartPos: $1.Start,
				EndPos: $7.End,
				Cond: $2,
				Then: $4,
				Else: $6,
			}
		}

then: THEN | NEWLINE

switch_statement:
	switch_stmt_cases END
		{
			n := $1
			n.EndPos = $2.End
			$$ = n
		}
	| switch_stmt_cases ELSE block END
		{
			n := $1
			n.Else = $3
			n.EndPos = $4.End
			$$ = n
		}

switch_stmt_cases:
	SWITCH opt_newlines CASE expression then block
		{
			$$ = &ast.SwitchStmt{
				StartPos: $1.Start,
				Cases: []ast.SwitchStmtCase{ {$4, $6} },
			}
		}
	| switch_stmt_cases CASE expression then block
		{
			n := $1
			n.Cases = append(n.Cases, ast.SwitchStmtCase{$3, $5})
			$$ = n
		}

match_statement:
	match_stmt_arms END
		{
			n := $1
			n.EndPos = $2.End
			$$ = n
		}
	| match_stmt_arms ELSE block END
		{
			n := $1
			n.Else = $3
			n.EndPos = $4.End
			$$ = n
		}

match_stmt_arms:
	MATCH expression opt_newlines WITH pattern then block
		{
			$$ = &ast.MatchStmt{
				StartPos: $1.Start,
				Matched: $2,
				Arms: []ast.MatchStmtArm{ {$5, $7} },
			}
		}
	| match_stmt_arms WITH pattern then block
		{
			n := $1
			n.Arms = append(n.Arms, ast.MatchStmtArm{$3, $5})
			$$ = n
		}

for_statement:
	FOR destructuring IN expression sep block END
		{
			$$ = &ast.ForEachStmt{
				StartPos: $1.Start,
				EndPos: $7.End,
				Iterator: $2,
				Range: $4,
				Body: $6,
			}
		}

while_statement:
	FOR expression sep block END
		{
			$$ = &ast.WhileStmt{
				StartPos: $1.Start,
				EndPos: $5.End,
				Cond: $2,
				Body: $4,
			}
		}

index_assign_statement:
	postfix_expr LBRACKET opt_newlines expression opt_newlines RBRACKET ASSIGN opt_newlines expression
		{
			$$ = &ast.IndexAssign{
				Assignee: $1,
				Index: $4,
				RHS: $9,
			}
		}

assign_statement:
	var_assign_lhs ASSIGN opt_newlines comma_sep_exprs
		{
			n := $1
			n.RHSExprs = $4
			$$ = n
		}

var_assign_lhs:
	IDENT
		{
			t := $1
			$$ = &ast.VarAssign{
				StartPos: t.Start,
				Idents: []ast.Symbol{ast.NewSymbol(t.Value())},
			}
		}
	| var_assign_lhs COMMA opt_newlines IDENT
		{
			n := $1
			n.Idents = append(n.Idents, ast.NewSymbol($4.Value()))
			$$ = n
		}

var_decl_statement:
	mutability destructurings ASSIGN opt_newlines comma_sep_exprs
		{
			mut := $1
			$$ = &ast.VarDecl{
				Decls: $2,
				RHSExprs: $5,
				StartPos: mut.Start,
				Mutable: mut.Kind == TokenVar,
			}
		}

mutability: VAR | LET

expr_statement:
	expression
		{
			$$ = &ast.ExprStmt{Expr: $1}
		}

destructurings:
	destructuring
		{
			$$ = []ast.Destructuring{ $1 }
		}
	| destructurings COMMA opt_newlines destructuring
		{
			$$ = append($1, $4)
		}

destructuring: record_destruct | var_destruct

/*
  Here 'LBRACE opt_newlines' is not available because it causes reduce/reduce conflict with
  record literal syntax. In the case '{ ident', parser need to look 2 tokens to know IDENT
  token because of LBRACE and opt_newlines.
  Instead of using opt_newlines, hack_record_lbrace notifys yyLexer to skip newlines.
*/
record_destruct:
	IDENT hack_record_lbrace LBRACE record_destruct_fields opt_newlines RBRACE
		{
			t := $1
			$$ = &ast.RecordDestructuring{
				StartPos: t.Start,
				EndPos: $6.End,
				Ident: ast.NewSymbol(t.Value()),
				Fields: $4,
			}
		}
	| hack_record_lbrace LBRACE record_destruct_fields opt_newlines RBRACE
		{
			$$ = &ast.RecordDestructuring{
				StartPos: $2.Start,
				EndPos: $5.End,
				Fields: $3,
			}
		}

hack_record_lbrace:
		{
			// Note:
			// This skips newlines **after** next token because parser looks next token
			// because YACC generates LALR(1) parser.
			// When this rule is reduced, next token is already eaten by parser to look ahead.
			// So this semantic action is called after next token is eaten, and after
			// next token, yyLexer starts to skip newlines.
			yylex.(*pseudoLexer).skipNewline = true
		}

record_destruct_fields:
	record_destruct_field
		{
			$$ = []ast.RecordDestructuringField{$1}
		}
	| record_destruct_fields elem_sep record_destruct_field
		{
			$$ = append($1, $3)
		}

record_destruct_field:
	IDENT
		{
			t := $1
			i := t.Value()
			$$ = ast.RecordDestructuringField{
				Name: i,
				Child: &ast.VarDeclDestructuring{
					StartPos: t.Start,
					EndPos: t.End,
					Ident: ast.NewSymbol(i),
				},
			}
		}
	| IDENT COLON destructuring
		{
			$$ = ast.RecordDestructuringField{
				Name: $1.Value(),
				Child: $3,
			}
		}

var_destruct:
	IDENT
		{
			t := $1
			$$ = &ast.VarDeclDestructuring{
				StartPos: t.Start,
				EndPos: t.End,
				Ident: ast.NewSymbol(t.Value()),
			}
		}

comma_sep_exprs:
	expression
		{
			$$ = []ast.Expression{$1}
		}
	| comma_sep_exprs COMMA opt_newlines expression
		{
			$$ = append($1, $4)
		}

expression:
	postfix_expr
	| expression AS opt_newlines type
		{
			$$ = &ast.CoerceExpr{
				Expr: $1,
				Type: $4,
			}
		}
	| expression STAR opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.MultOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression DIV opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.DivOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression PERCENT opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.ModOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression PLUS opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.AddOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression MINUS opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.SubOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression LESS opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.LessOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression LESSEQUAL opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.LessEqOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression GREATER opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.GreaterOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression GREATEREQUAL opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.GreaterEqOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression EQUAL opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.EqOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression NOTEQUAL opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.NotEqOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression AND opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.AndOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| expression OR opt_newlines expression
		{
			$$ = &ast.BinaryExpr{
				Op: ast.OrOp,
				LHS: $1,
				RHS: $4,
			}
		}
	| PLUS expression %prec prec_unary
		{
			$$ = &ast.UnaryExpr{
				StartPos: $1.Start,
				Op: ast.PositiveOp,
				Child: $2,
			}
		}
	| MINUS expression %prec prec_unary
		{
			$$ = &ast.UnaryExpr{
				StartPos: $1.Start,
				Op: ast.NegOp,
				Child: $2,
			}
		}
	| NOT expression %prec prec_unary
		{
			$$ = &ast.UnaryExpr{
				StartPos: $1.Start,
				Op: ast.NotOp,
				Child: $2,
			}
		}
	| if_statement
		{
			e, err := ifExpr($1)
			if err != nil {
				yylex.Error(err.Error())
			}
			$$ = e
		}
	| switch_statement
		{
			e, err := switchExpr($1)
			if err != nil {
				yylex.Error(err.Error())
			}
			$$ = e
		}
	| match_statement
		{
			e, err := matchExpr($1)
			if err != nil {
				yylex.Error(err.Error())
			}
			$$ = e
		}

postfix_expr:
	primary_expr
	| postfix_expr LBRACKET opt_newlines expression opt_newlines RBRACKET
		{
			$$ = &ast.IndexAccess{
				EndPos: $6.End,
				Child: $1,
				Index: $4,
			}
		}
	| postfix_expr DOT opt_newlines IDENT
		{
			i := $4
			$$ = &ast.FieldAccess{
				EndPos: i.End,
				Child: $1,
				Name: i.Value(),
			}
		}
	| postfix_expr LPAREN opt_newlines func_call_named_args opt_newlines RPAREN opt_do_end_block
		{
			$$ = &ast.FuncCallNamed{
				EndPos: $6.End,
				Callee: $1,
				Args: $4,
				DoBlock: $7,
			}
		}
	| postfix_expr LPAREN opt_newlines func_call_args opt_newlines RPAREN opt_do_end_block
		{
			$$ = &ast.FuncCall{
				EndPos: $6.End,
				Callee: $1,
				Args: $4,
				DoBlock: $7,
			}
		}

opt_do_end_block:
		{
			$$ = nil
		}
	| DO lambda_params_in block END
		{
			params := $2
			// Reverse 'params'
			len := len(params)
			for i,p := range params[:len/2] {
				j := len - 1 - i
				params[i] = params[j]
				params[j] = p
			}

			e, err := blockExpr($3, $1.Start)
			if err != nil {
				yylex.Error(err.Error())
			}

			$$ = &ast.Lambda{
				StartPos: $1.Start,
				EndPos: $4.End,
				IsDoBlock: true,
				Params: params,
				BodyExpr: e,
			}
		}

func_call_named_args:
	IDENT COLON expression
		{
			$$ = []ast.NamedArg{ {$1.Value(), $3} }
		}
	| func_call_named_args COMMA opt_newlines IDENT COLON expression
		{
			$$ = append($1, ast.NamedArg{$4.Value(), $6})
		}

func_call_args:
	expression
		{
			$$ = []ast.Expression{$1}
		}
	| func_call_args COMMA opt_newlines expression
		{
			$$ = append($1, $4)
		}

primary_expr:
	record_or_tuple_literal
	| array_literal
	| dict_literal
	| lambda_expr
	| var_ref
	| constant
	| LPAREN opt_newlines expression opt_newlines RPAREN { $$ = $3 }

record_or_tuple_literal:
	IDENT record_or_tuple_anonym
		{
			i, r := $1, $2
			switch r := r.(type) {
			case *ast.RecordLiteral:
				r.Ident = ast.NewSymbol(i.Value())
				r.StartPos = i.Start
			case *ast.TupleLiteral:
				r.Ident = ast.NewSymbol(i.Value())
				r.StartPos = i.Start
			default:
				yylex.Error("FATAL: record_or_tuple_anonym is not record nor tuple: " + r.String())
			}
			$$ = r
		}
	| record_or_tuple_anonym

record_or_tuple_anonym:
	LBRACE opt_newlines record_literal_fields opt_newlines RBRACE
		{
			fields := $3
			if fields[0].Name == "_" {
				// Tuple
				elems := make([]ast.Expression, 0, len(fields))
				elems = append(elems, fields[0].Expr)
				for i, f := range fields[1:] {
					if f.Name != "_" {
						yylex.Error(fmt.Sprintf("Mixing unnamed and named fields is not permitted. %s field must be unnamed at %s", prelude.Ordinal(i+2), $1.String()))
						break
					}
					elems = append(elems, f.Expr)
				}
				$$ = &ast.TupleLiteral{
					StartPos: $1.Start,
					EndPos: $5.End,
					Elems: elems,
				}
			} else {
				for i, f := range fields[1:] {
					if f.Name == "_" {
						yylex.Error(fmt.Sprintf("Mixing unnamed and named fields is not permitted. %s field must be named at %s", prelude.Ordinal(i+2), $1.String()))
						break
					}
				}
				$$ = &ast.RecordLiteral{
					StartPos: $1.Start,
					EndPos: $5.End,
					Fields: $3,
				}
			}
		}
	| LBRACE opt_newlines RBRACE
		{
			$$ = &ast.TupleLiteral{
				StartPos: $1.Start,
				EndPos: $3.End,
			}
		}

record_literal_fields:
	record_literal_field
		{
			$$ = []ast.RecordLitField{$1}
		}
	| record_literal_fields elem_sep record_literal_field
		{
			$$ = append($1, $3)
		}

record_literal_field:
	IDENT
		{
			t := $1
			i := t.Value()
			$$ = ast.RecordLitField{
				Name: i,
				Expr: &ast.VarRef{
					StartPos: t.Start,
					EndPos: t.End,
					Ident: ast.NewSymbol(i),
				},
			}
		}
	| IDENT COLON opt_newlines expression
		{
			$$ = ast.RecordLitField{
				Name: $1.Value(),
				Expr: $4,
			}
		}

array_literal:
	LBRACKET opt_newlines array_elems opt_newlines RBRACKET
		{
			$$ = &ast.ArrayLiteral{
				StartPos: $1.Start,
				EndPos: $5.End,
				Elems: $3,
			}
		}
	| LBRACKET opt_newlines RBRACKET
		{
			$$ = &ast.ArrayLiteral{
				StartPos: $1.Start,
				EndPos: $3.End,
			}
		}

array_elems:
	expression
		{
			$$ = []ast.Expression{$1}
		}
	| array_elems elem_sep expression
		{
			$$ = append($1, $3)
		}

dict_literal:
	LBRACKET opt_newlines dict_elems opt_newlines RBRACKET
		{
			$$ = &ast.DictLiteral{
				StartPos: $1.Start,
				EndPos: $5.End,
				Elems: $3,
			}
		}
	| LBRACKET opt_newlines FATRIGHTARROW opt_newlines RBRACKET
		{
			$$ = &ast.DictLiteral{
				StartPos: $1.Start,
				EndPos: $5.End,
			}
		}

dict_elems:
	expression FATRIGHTARROW expression
		{
			$$ = []ast.DictKeyVal{
				{$1, $3},
			}
		}
	| dict_elems elem_sep expression FATRIGHTARROW expression
		{
			$$ = append($1, ast.DictKeyVal{$3, $5})
		}

/*
  func_params cannot be used for this rule because it causes reduce/reduce conflict with var_ref.
  This is because parser cannot know which rule should be reduced after IDENT. So we need to specify
  some token after IDENT token. Here, IN or DO comes after parameters. lambda_params_in and
  lambda_params_do informs parser of what comes after IDENT token to avoid conflict.
*/
lambda_expr:
	RIGHTARROW lambda_params_in expression %prec prec_lambda
		{
			params := $2
			// Reverse 'params'
			len := len(params)
			for i,p := range params[:len/2] {
				j := len - 1 - i
				params[i] = params[j]
				params[j] = p
			}

			$$ = &ast.Lambda{
				StartPos: $1.Start,
				IsDoBlock: false,
				Params: params,
				BodyExpr: $3,
			}
		}
	| RIGHTARROW expression %prec prec_lambda
		{
			$$ = &ast.Lambda{
				StartPos: $1.Start,
				IsDoBlock: false,
				BodyExpr: $2,
			}
		}
	| RIGHTARROW lambda_params_do
		block
	END
		{
			params := $2
			// Reverse 'params'
			len := len(params)
			for i,p := range params[:len/2] {
				j := len - 1 - i
				params[i] = params[j]
				params[j] = p
			}

			e, err := blockExpr($3, $1.Start)
			if err != nil {
				yylex.Error(err.Error())
			}

			$$ = &ast.Lambda{
				StartPos: $1.Start,
				EndPos: $4.End,
				IsDoBlock: true,
				Params: params,
				BodyExpr: e,
			}
		}
	| RIGHTARROW DO
		block
	END
		{
			e, err := blockExpr($3, $1.Start)
			if err != nil {
				yylex.Error(err.Error())
			}

			$$ = &ast.Lambda{
				StartPos: $1.Start,
				EndPos: $4.End,
				IsDoBlock: true,
				BodyExpr: e,
			}
		}

/* Unlike function parameters, newline cannot be used for separater. */
lambda_params_in:
	IDENT IN
		{
			$$ = []ast.FuncParam{
				{ast.NewSymbol($1.Value()), nil},
			}
		}
	| IDENT COLON type IN
		{
			$$ = []ast.FuncParam{
				{ast.NewSymbol($1.Value()), $3},
			}
		}
	| IDENT COMMA lambda_params_in
		{
			$$ = append($3, ast.FuncParam{ast.NewSymbol($1.Value()), nil})
		}
	| IDENT COLON type COMMA lambda_params_in
		{
			$$ = append($5, ast.FuncParam{ast.NewSymbol($1.Value()), $3})
		}

lambda_params_do:
	IDENT DO
		{
			$$ = []ast.FuncParam{
				{ast.NewSymbol($1.Value()), nil},
			}
		}
	| IDENT COLON type DO
		{
			$$ = []ast.FuncParam{
				{ast.NewSymbol($1.Value()), $3},
			}
		}
	| IDENT COMMA lambda_params_do
		{
			$$ = append($3, ast.FuncParam{ast.NewSymbol($1.Value()), nil})
		}
	| IDENT COLON type COMMA lambda_params_do
		{
			$$ = append($5, ast.FuncParam{ast.NewSymbol($1.Value()), $3})
		}

var_ref:
	IDENT
		{
			t := $1
			$$ = &ast.VarRef{
				StartPos: t.Start,
				EndPos: t.End,
				Ident: ast.NewSymbol(t.Value()),
			}
		}

constant:
	INT
		{
			t := $1
			v, s, err := tokenToInt(t.Value())
			if err != nil {
				yylex.Error(fmt.Sprintf("Parse error at integer literal '%s': %s", t.Value(), err.Error()))
			} else if s {
				$$ = &ast.IntLiteral{
					StartPos: t.Start,
					EndPos: t.End,
					Value: int64(v),
				}
			} else {
				$$ = &ast.UIntLiteral{
					StartPos: t.Start,
					EndPos: t.End,
					Value: v,
				}
			}
		}
	| FLOAT
		{
			t := $1
			f, err := strconv.ParseFloat(t.Value(), 64)
			if err != nil  {
				yylex.Error(fmt.Sprintf("Parse error at float literal '%s': %s", t.Value(), err.Error()))
			} else {
				$$ = &ast.FloatLiteral{
					StartPos: t.Start,
					EndPos: t.End,
					Value: f,
				}
			}
		}
	| BOOL
		{
			t := $1
			$$ = &ast.BoolLiteral{
				StartPos: t.Start,
				EndPos: t.End,
				Value: t.Value() == "true",
			}
		}
	| STRING
		{
			t := $1
			s, err := strconv.Unquote(t.Value())
			if err != nil {
				yylex.Error(fmt.Sprintf("Parse error at string literal '%s': %s", t.Value(), err.Error()))
			} else {
				$$ = &ast.StringLiteral{
					StartPos: t.Start,
					EndPos: t.End,
					Value: s,
				}
			}
		}

pattern:
	const_pattern | record_pattern | array_pattern | var_pattern

const_pattern:
	INT
		{
			t := $1
			v, s, err := tokenToInt(t.Value())
			if err != nil {
				yylex.Error("Parse error at integer pattern: " + err.Error())
			} else if s {
				$$ = &ast.IntConstPattern{
					StartPos: t.Start,
					EndPos: t.End,
					Value: int64(v),
				}
			} else {
				$$ = &ast.UIntConstPattern{
					StartPos: t.Start,
					EndPos: t.End,
					Value: v,
				}
			}
		}
	| BOOL
		{
			t := $1
			$$ = &ast.BoolConstPattern{
				StartPos: t.Start,
				EndPos: t.End,
				Value: t.Value() == "true",
			}
		}
	| STRING
		{
			t := $1
			s, err := strconv.Unquote(t.Value())
			if err != nil {
				yylex.Error(fmt.Sprintf("Parse error at string pattern '%s': %s", t.Value(), err.Error()))
			} else {
				$$ = &ast.StringConstPattern{
					StartPos: t.Start,
					EndPos: t.End,
					Value: s,
				}
			}
		}
	| FLOAT
		{
			t := $1
			f, err := strconv.ParseFloat(t.Value(), 64)
			if err != nil  {
				yylex.Error("Parse error at float pattern: " + err.Error())
			} else {
				$$ = &ast.FloatConstPattern{
					StartPos: t.Start,
					EndPos: t.End,
					Value: f,
				}
			}
		}

var_pattern:
	IDENT
		{
			t := $1
			$$ = &ast.VarDeclPattern{
				StartPos: t.Start,
				EndPos: t.End,
				Ident: ast.NewSymbol(t.Value()),
			}
		}

record_pattern:
	IDENT rec_pat_anonym
		{
			n := $2
			t := $1
			n.StartPos = t.Start
			n.Ident = ast.NewSymbol(t.Value())
			$$ = n
		}
	| rec_pat_anonym
		{
			$$ = $1
		}

rec_pat_anonym:
	LBRACE opt_newlines rec_pat_fields opt_newlines RBRACE
		{
			$$ = &ast.RecordPattern{
				StartPos: $1.Start,
				EndPos: $5.End,
				Fields: $3,
			}
		}
	| LBRACE opt_newlines RBRACE
		{
			$$ = &ast.RecordPattern{
				StartPos: $1.Start,
				EndPos: $3.End,
			}
		}

rec_pat_fields:
	rec_pat_field
		{
			$$ = []ast.RecordPatternField{$1}
		}
	| rec_pat_fields elem_sep rec_pat_field
		{
			$$ = append($1, $3)
		}

rec_pat_field:
	IDENT
		{
			t := $1
			i := t.Value()
			$$ = ast.RecordPatternField{
				Name: i,
				Pattern:&ast.VarDeclPattern{
					StartPos: t.Start,
					EndPos: t.End,
					Ident: ast.NewSymbol(i),
				},
			}
		}
	| IDENT COLON opt_newlines pattern
		{
			$$ = ast.RecordPatternField{
				Name: $1.Value(),
				Pattern: $4,
			}
		}

array_pattern:
	LBRACKET opt_newlines array_pat_elems opt_newlines opt_exhaustive_pattern RBRACKET
		{
			$$ = &ast.ArrayPattern{
				StartPos: $1.Start,
				EndPos: $6.End,
				Elems: $3,
				Exhaustive: $5,
			}
		}
	| LBRACKET opt_newlines opt_exhaustive_pattern RBRACKET
		{
			$$ = &ast.ArrayPattern{
				StartPos: $1.Start,
				EndPos: $4.End,
				Exhaustive: $3,
			}
		}

array_pat_elems:
	pattern
		{
			$$ = []ast.Pattern{$1}
		}
	| array_pat_elems elem_sep pattern
		{
			$$ = append($1, $3)
		}

opt_exhaustive_pattern:
		{
			$$ = false
		}
	| ELLIPSIS opt_newlines
		{
			$$ = true
		}

/* TODO: Add DictPattern and ArrayPattern */

/*
  aaa,bbb,ccc

  aaa
  bbb
  ccc

  aaa,bbb
  ccc
*/
sep_names:
	IDENT
		{
			$$ = []string{$1.Value()}
		}
	| sep_names elem_sep IDENT
		{
			$$ = append($1, $3.Value())
		}

elem_sep:
	COMMA | newlines

newlines:
	NEWLINE
	| NEWLINE newlines

opt_newlines: {} | newlines {}

seps:
   SEMICOLON
   | NEWLINE
   | SEMICOLON seps
   | NEWLINE seps

sep: SEMICOLON | NEWLINE

%%

func tokenToInt(s string) (val uint64, sign bool, err error) {
	sign = true
	bits := 63

	if strings.HasSuffix(s, "u") {
		s = s[:len(s)-1]
		sign = false
		bits = 64
	}

	base := 10
	if strings.HasPrefix(s, "0x") {
		base, s = 16, s[2:]
	} else if strings.HasPrefix(s, "0b") {
		base, s = 2, s[2:]
	}

	val, err = strconv.ParseUint(s, base, bits)
	return
}

// vim: noet
