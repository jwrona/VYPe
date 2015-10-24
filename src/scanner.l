%{
#include "common.h" //eturn codes
#include "data_type.h" //have to be here
#include "parser.h" //generated by bison
%}

%option warn
%option noyywrap
 /* pairs with api.pure */
%option bison-bridge

%x ML_COMMENT

ESC \\[nt\\"']
CHAR [[:print:]]{-}[\\"']

%%

 /* Multi-line comments. */
"/*" { BEGIN(ML_COMMENT); }
<ML_COMMENT>{
        "*/" BEGIN(INITIAL);
        [^*\n]+ ;
        "*"[^/] ;
        \n ;
}

 /* Single-line comments. */
"//".* { }


 /* Keywords: */
"char" { return CHAR; }
"else" { return ELSE; }
"if" { return IF; }
"int" { return INT; }
"return" { return RETURN; }
"string" { return STRING; }
"void" { return VOID; }
"while" { return WHILE; }

 /* Reserved words: */
"break" { return BREAK; }
"continue" { return CONTINUE; }
"for" { return FOR; }
"short" { return SHORT; }
"unsigned" { return UNSIGNED; }

 /* Identifier. TODO: free(). */
[a-zA-Z_][a-zA-Z_0-9]* { yylval->identifier = strdup(yytext); return IDENTIFIER; }


 /* Integer literal (decimal only). */
[[:digit:]]+ { yylval->int_lit = atoi(yytext); return INT_LIT; }

 /* Character literal. */
'({ESC}|{CHAR})' { yylval->char_lit = yytext[0]; return CHAR_LIT; }

 /* String literal. TODO: free() */
\"({ESC}|{CHAR})*\" { yylval->string_lit = strdup(yytext); return STRING_LIT; }


 /* Other characters. */
"(" { return '('; }
")" { return ')'; }
"{" { return '{'; }
"}" { return '}'; }
"," { return ','; }
";" { return ';'; }
"=" { return '='; }

 /* Operators. */
"!" { return '!'; }
"*" { return '*'; }
"/" { return '/'; }
"%" { return '%'; }
"+" { return '+'; }
"-" { return '-'; }
"<" { return '<'; }
">" { return '>';}
"<=" { return LE_OP;}
">=" { return GE_OP;}
"==" { return EQ_OP;}
"!=" { return NE_OP;}
"&&" { return AND_OP;}
"||" { return OR_OP;}


 /* Eat up whitespaces. */
[[:space:]]+ { }

 /* Unknown. TODO: error recovery? */
. { fprintf(stderr, "lexical error: unexpected symbol '%c'\n", yytext[0]); exit(RET_LEXICAL); }

%%