%{
package syntax

// import (
//     "github.com/rhysd/Dachs/next/compiler/ast"
// )
%}

%union{
    token *Token
}

%token<token> ILLEGAL

%type<> program

%start program

%%

program: ILLEGAL
    {
        yylex.(*pseudoLexer).result = $1
    }

%%
