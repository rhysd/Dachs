package syntax

import (
	"github.com/rhysd/Dachs/next/compiler/ast"
	"github.com/rhysd/Dachs/next/compiler/prelude"
)

// This file provides one function blockExpr(stmts: []ast.Statement) (expr ast.Expression, err error).
// It converts a statements block into one expression (usually ast.SeqExpr).
// This function is necessary because it is hard to handle below rule in LALR(1) without conflicts.
//
//   stmts_expr: statements expression | expression
//
// statements can eat one expression as ExprStatement. So the rule never be reduced and will cause
// an error. So we need to parse body of expression block with 'statements' and then convert the
// last statement to expression. If there is no statement in the 'statements' block or last
// statement cannot be converted into expression. Parser should raise an error.
//
// The conversion is needed to be done with recursive process because the last expression may
// contain another expression block.
// e.g.
//   if true then 1 else if true then 2 else 3 end else 4 end
// The last expression is outer 'if' statement (will be converted into 'if' expression). It contains
// another 'if' statement in 'else' block. It should be also converted into expression recursively.

func ifExpr(stmt *ast.IfStmt) (ast.Expression, error) {
	thenExpr, err := blockExpr(stmt.Then, stmt.StartPos)
	if err != nil {
		return nil, prelude.Wrap(stmt.Pos(), stmt.End(), err, "'then' block of 'if' expression is incorrect")
	}

	elseExpr, err := blockExpr(stmt.Else, stmt.StartPos)
	if err != nil {
		return nil, prelude.Wrap(stmt.Pos(), stmt.End(), err, "'else' block of 'if' expression is incorrect")
	}

	return &ast.IfExpr{
		StartPos: stmt.StartPos,
		EndPos:   stmt.EndPos,
		Cond:     stmt.Cond,
		Then:     thenExpr,
		Else:     elseExpr,
	}, nil
}

func switchExpr(stmt *ast.SwitchStmt) (ast.Expression, error) {
	cases := make([]ast.SwitchExprCase, 0, len(stmt.Cases))
	for i, c := range stmt.Cases {
		e, err := blockExpr(c.Stmts, c.StartPos)
		if err != nil {
			return nil, prelude.Wrapf(stmt.Pos(), stmt.End(), err, "%dth 'case' block is incorrect", i+1)
		}
		cases = append(cases, ast.SwitchExprCase{
			Cond: c.Cond,
			Body: e,
		})
	}

	elseExpr, err := blockExpr(stmt.Else, stmt.Pos())
	if err != nil {
		return nil, prelude.Wrap(stmt.Pos(), stmt.End(), err, "'else' block of 'case' expression is incorrect")
	}

	return &ast.SwitchExpr{
		EndPos: stmt.EndPos,
		Cases:  cases,
		Else:   elseExpr,
	}, nil
}

func matchExpr(stmt *ast.MatchStmt) (ast.Expression, error) {
	cases := make([]ast.MatchExprCase, 0, len(stmt.Cases))
	for i, c := range stmt.Cases {
		e, err := blockExpr(c.Stmts, stmt.StartPos)
		if err != nil {
			return nil, prelude.Wrapf(stmt.Pos(), stmt.End(), err, "%dth 'case' block of 'match' expression is incorrect", i+1)
		}
		cases = append(cases, ast.MatchExprCase{
			Pattern: c.Pattern,
			Body:    e,
		})
	}

	elseExpr, err := blockExpr(stmt.Else, stmt.StartPos)
	if err != nil {
		return nil, prelude.Wrap(stmt.Pos(), stmt.End(), err, "'else' block of 'match' expression is incorrect")
	}

	return &ast.MatchExpr{
		StartPos: stmt.StartPos,
		EndPos:   stmt.EndPos,
		Matched:  stmt.Matched,
		Cases:    cases,
		Else:     elseExpr,
	}, nil
}

func promoteStmtToExpr(stmt ast.Statement) (ast.Expression, error) {
	switch stmt := stmt.(type) {
	case *ast.ExprStmt:
		return stmt.Expr, nil
	case *ast.IfStmt:
		return ifExpr(stmt)
	case *ast.SwitchStmt:
		return switchExpr(stmt)
	case *ast.MatchStmt:
		return matchExpr(stmt)
	default:
		return nil, prelude.NewErrorf(stmt.Pos(), stmt.End(), "Blocks in expression must end with an expression but the last item in block is '%s'", stmt.String())
	}
}

func blockExpr(stmts []ast.Statement, hint prelude.Pos) (ast.Expression, error) {
	if len(stmts) == 0 {
		return nil, prelude.NewErrorAt(hint, "Blocks in expression must end with an expression, but the block is empty")
	}

	e, err := promoteStmtToExpr(stmts[len(stmts)-1])
	if err != nil {
		return nil, err
	}

	if len(stmts) == 1 {
		return e, nil
	}

	return &ast.SeqExpr{
		Stmts:    stmts[:len(stmts)-1],
		LastExpr: e,
	}, nil
}
