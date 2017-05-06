%{

package syntax

import (
	"fmt"
	"strconv"
	"strings"
	"github.com/rhysd/Dachs/next/compiler/ast"
)
%}

%union{
	token *Token
	node ast.Node
	nodes []ast.Node
	stmt ast.Statement
	stmts []ast.Statement
	type_node ast.Type
	type_nodes []ast.Type
	import_node *ast.Import
	names []string
	params []ast.FuncParam
	switch_stmt_cases []ast.SwitchStmtCase
	match_stmt_cases []ast.MatchStmtCase
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
	switch_expr_cases []ast.SwitchExprCase
	match_expr_cases []ast.MatchExprCase
	record_lit_fields []ast.RecordLitField
	record_lit_field ast.RecordLitField
	dict_keyvals []ast.DictKeyVal
	named_args []ast.NamedArg
	lambda *ast.Lambda
	enum_cases []ast.EnumTypeCase
	bool bool
}

%token<token> ILLEGAL
%token<token> COMMENT
%token<token> NEWLINE
%token<token> SEMICOLON
%token<token> LPAREN
%token<token> RPAREN
%token<token> LBRACE
%token<token> RBRACE
%token<token> LBRACKET
%token<token> RBRACKET
%token<token> IDENT
%token<token> BOOL
%token<token> INT
%token<token> FLOAT
%token<token> STRING
%token<token> MINUS
%token<token> PLUS
%token<token> STAR
%token<token> DIV
%token<token> NOT
%token<token> OR
%token<token> AND
%token<token> PERCENT
%token<token> EQUAL
%token<token> NOTEQUAL
%token<token> ASSIGN
%token<token> LESS
%token<token> LESSEQUAL
%token<token> GREATER
%token<token> GREATEREQUAL
%token<token> END
%token<token> IF
%token<token> THEN
%token<token> ELSE
%token<token> CASE
%token<token> MATCH
%token<token> RET
%token<token> IMPORT
%token<token> DOT
%token<token> TYPE
%token<token> COLON
%token<token> FOR
%token<token> IN
%token<token> FATRIGHTARROW
%token<token> COLONCOLON
%token<token> TYPEOF
%token<token> AS
%token<token> FUNC
%token<token> DO
%token<token> RIGHTARROW
%token<token> COMMA
%token<token> BREAK
%token<token> NEXT
%token<token> ELLIPSIS
%token<token> SINGLEQUOTE
%token<token> VAR
%token<token> LET
%token<token> EOF

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
%type<type_node> type opt_type_annotate
%type<stmts> statements opt_stmts
%type<stmt> statement if_statement ret_statement switch_statement match_statement var_decl_statement expr_statement for_statement while_statement index_assign_statement assign_statement
%type<var_assign_stmt> var_assign_lhs
%type<switch_stmt_cases> switch_stmt_cases
%type<match_stmt_cases> match_stmt_cases
%type<expr> expression constant postfix_expr primary_expr if_expr switch_expr match_expr record_or_tuple_literal record_or_tuple_anonym var_ref array_literal dict_literal lambda_expr stmts_expr
%type<exprs> ret_body comma_sep_exprs array_elems func_call_args
%type<switch_expr_cases> switch_expr_cases
%type<match_expr_cases> match_expr_cases
%type<record_lit_fields> record_literal_fields
%type<record_lit_field> record_literal_field
%type<params> func_params lambda_params_in lambda_params_do
%type<type_node> type_ref type_var type_instantiate type_record_or_tuple type_func type_typeof
%type<type_fields> type_record_fields
%type<type_nodes> opt_types types
%type<destructuring> destructuring var_destruct record_destruct
%type<destructurings> destructurings
%type<rec_destruct_fields> record_destruct_fields
%type<rec_destruct_field> record_destruct_field
%type<pattern> pattern const_pattern var_pattern record_pattern array_pattern
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
	FUNC IDENT opt_type_annotate sep opt_stmts END
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
	| FUNC IDENT LPAREN opt_newlines RPAREN opt_type_annotate sep opt_stmts END
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
	| FUNC IDENT LPAREN opt_newlines func_params opt_newlines RPAREN opt_type_annotate sep opt_stmts END
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

statements:
	statement
		{
			s := $1
			if s == nil {
				$$ = []ast.Statement{}
			} else {
				$$ = []ast.Statement{$1}
			}
		}
	| statements statement
		{
			s := $2
			if s != nil {
				$$ = append($1, s)
			}
		}

opt_stmts:
		{
			$$ = []ast.Statement{}
		}
	| statements

statement:
	ret_statement
	| if_statement
	| switch_statement
	| match_statement
	| for_statement
	| while_statement
	| index_assign_statement
	| assign_statement
	| var_decl_statement
	| expr_statement
	| sep { $$ = nil }

ret_statement:
	RET ret_body sep
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
	IF expression then opt_stmts END
		{
			$$ = &ast.IfStmt{
				StartPos: $1.Start,
				EndPos: $5.End,
				Cond: $2,
				Then: $4,
			}
		}
	| IF expression then opt_stmts ELSE opt_stmts END
		{
			$$ = &ast.IfStmt{
				StartPos: $1.Start,
				EndPos: $7.End,
				Cond: $2,
				Then: $4,
				Else: $6,
			}
		}

then: THEN | THEN newlines | newlines

switch_statement:
	switch_stmt_cases END
		{
			$$ = &ast.SwitchStmt{
				EndPos: $2.End,
				Cases: $1,
			}
		}
	| switch_stmt_cases ELSE opt_stmts END
		{
			$$ = &ast.SwitchStmt{
				EndPos: $4.End,
				Cases: $1,
				Else: $3,
			}
		}

switch_stmt_cases:
	CASE expression then opt_stmts
		{
			$$ = []ast.SwitchStmtCase{ {$1.Start, $2, $4} }
		}
	| switch_stmt_cases CASE expression then opt_stmts
		{
			$$ = append($1, ast.SwitchStmtCase{$2.Start, $3, $5})
		}

match_statement:
	MATCH expression seps match_stmt_cases END
		{
			$$ = &ast.MatchStmt{
				StartPos: $1.Start,
				EndPos: $5.End,
				Matched: $2,
				Cases: $4,
			}
		}
	| MATCH expression seps match_stmt_cases ELSE opt_stmts END
		{
			$$ = &ast.MatchStmt{
				StartPos: $1.Start,
				EndPos: $7.End,
				Matched: $2,
				Cases: $4,
				Else: $6,
			}
		}

match_stmt_cases:
	CASE pattern then opt_stmts
		{
			$$ = []ast.MatchStmtCase{ {$2, $4} }
		}
	| match_stmt_cases CASE pattern then opt_stmts
		{
			$$ = append($1, ast.MatchStmtCase{$3, $5})
		}

for_statement:
	FOR destructuring IN expression sep opt_stmts END
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
	FOR expression sep opt_stmts END
		{
			$$ = &ast.WhileStmt{
				StartPos: $1.Start,
				EndPos: $5.End,
				Cond: $2,
				Body: $4,
			}
		}

index_assign_statement:
	postfix_expr LBRACKET opt_newlines expression opt_newlines RBRACKET ASSIGN opt_newlines expression sep
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
	mutability destructurings ASSIGN opt_newlines comma_sep_exprs sep
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
	expression sep
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
	| DO lambda_params_in opt_newlines stmts_expr opt_newlines END
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
				EndPos: $6.End,
				IsDoBlock: true,
				Params: params,
				BodyExpr: $4,
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
	if_expr
	| switch_expr
	| match_expr
	| record_or_tuple_literal
	| array_literal
	| dict_literal
	| lambda_expr
	| var_ref
	| constant
	| LPAREN opt_newlines expression opt_newlines RPAREN { $$ = $3 }

stmts_expr:
	statements expression
		{
			$$ = &ast.SeqExpr{Stmts: $1, LastExpr: $2}
		}
	| expression

if_expr:
	IF expression then
		stmts_expr
	ELSE
		stmts_expr opt_newlines
	END
		{
			$$ = &ast.IfExpr{
				StartPos: $1.Start,
				EndPos: $8.End,
				Cond: $2,
				Then: $4,
				Else: $6,
			}
		}

switch_expr:
	switch_expr_cases opt_newlines ELSE stmts_expr opt_newlines END
		{
			$$ = &ast.SwitchExpr{
				EndPos: $6.End,
				Cases: $1,
				Else: $4,
			}
		}

switch_expr_cases:
	CASE expression then stmts_expr
		{
			$$ = []ast.SwitchExprCase{
				{$1.Start, $2, $4},
			}
		}
	| switch_expr_cases seps CASE expression then stmts_expr
		{
			$$ = append($1, ast.SwitchExprCase{$3.Start, $4, $6})
		}

match_expr:
	MATCH expression seps match_expr_cases opt_newlines ELSE stmts_expr opt_newlines END
		{
			$$ = &ast.MatchExpr{
				StartPos: $1.Start,
				EndPos: $9.End,
				Matched: $2,
				Cases: $4,
				Else: $7,
			}
		}

match_expr_cases:
	CASE pattern then stmts_expr
		{
			$$ = []ast.MatchExprCase{
				{$2, $4},
			}
		}
	| match_expr_cases seps CASE pattern then stmts_expr
		{
			$$ = append($1, ast.MatchExprCase{$4, $6})
		}

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
						yylex.Error(fmt.Sprintf("Mixing unnamed and named fields is not permitted. %dth field must be unnamed at %s", i+2, $1.String()))
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
						yylex.Error(fmt.Sprintf("Mixing unnamed and named fields is not permitted. %dth field must be named at %s", i+2, $1.String()))
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
	| RIGHTARROW lambda_params_do opt_newlines
		stmts_expr opt_newlines
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

			$$ = &ast.Lambda{
				StartPos: $1.Start,
				EndPos: $6.End,
				IsDoBlock: true,
				Params: params,
				BodyExpr: $4,
			}
		}
	| RIGHTARROW DO opt_newlines
		stmts_expr opt_newlines
	END
		{
			$$ = &ast.Lambda{
				StartPos: $1.Start,
				EndPos: $6.End,
				IsDoBlock: true,
				BodyExpr: $4,
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