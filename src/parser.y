%{
/* Prologue. */
#include "common.h"
#include "data_type.h"
#include "hash_table.h"


#include <stdio.h>
#include <assert.h>


typedef enum {
        FUNC_STATE_UNSET,
        FUNC_STATE_DECLARED,
        FUNC_STATE_DEFINED,
} func_state_t;

struct block_record {
        data_type_t symbol_type; //int, char, string or function

        /* Function specific variables. */
        func_state_t func_state; //declared or defined
        struct var_list *var_list; //return type and parameter list
        size_t params_cnt; //could be merged into var_list
        data_type_t ret_type;
};

struct block {
        struct hash_table *symbol_table;
        struct block *prev;
};


int yylex(); //defined in scanner.h

/* Error handling declarations. */
static void yyerror (const int *return_code, char const *s);
static void set_error(int code, const char *id, const char *message);

/* Block (chained symbol tables) related declarations. */
static struct block * block_init(struct block *prev);
static struct block * block_free(struct block *block);
static void * block_put(struct block *block, const char *id,
                 const struct block_record *br);
static struct block_record * block_get(const struct block *block,
                                       const char *id);

/* Semantic actions declarations. */
static int sem_function_declaration(const char *id, data_type_t ret_type,
                              struct var_list *type_list);
static int sem_function_definition(char *id, data_type_t ret_type,
                            struct var_list *type_list);
int sem_variable_definition_statement(data_type_t data_type,
                                      struct var_list *id_list);
int sem_function_call(char *id, size_t params_cnt, data_type_t *ret_dt);

int sem_expr_identifier(char *id, data_type_t *data_type);
int sem_expr_cast(data_type_t dt_to, data_type_t dt_from);


struct block *top_block = NULL;
int *glob_return_code;
size_t expr_res = 0;
%}

/* pairs with bison-bridge */
%define api.pure
%error-verbose
%parse-param {int *return_code}

/*---------------------
| Bison declarations. |
---------------------*/
%union {
        char *identifier;
        int int_lit;
        char char_lit;
        char *string_lit;
        data_type_t data_type;
        struct var_list *var_list;
        size_t expression_cnt;
}

/* Tokens. */
%token ELSE IF RETURN WHILE
%token BREAK CONTINUE FOR SHORT UNSIGNED
%token <identifier> IDENTIFIER
%token INT CHAR STRING
%token VOID
%token <int_lit> INT_LIT
%token <char_lit> CHAR_LIT
%token <string_lit> STRING_LIT

/* Nonterminals. */
%type <data_type> data_type type
%type <data_type> expression
%type <data_type> function_call
%type <var_list> parameter_type_list parameter_identifier_list
%type <var_list> identifier_list
%type <expression_cnt> expression_list

/* Operator associativity and precedence. */
%left OR_OP
%left AND_OP
%left EQ_OP NE_OP
%left '<' '>' LE_OP GE_OP
%left '+' '-'
%left '*' '/' '%'
%left NEG /* phony terminal symbol for unary negation '!' */
%nonassoc '(' ')'

%initial-action
{
        glob_return_code = return_code;

        /* Create block for functions and global variables. */
        top_block = block_init(top_block);
        if (top_block == NULL) {
                YYERROR;
        }
}

%%
/*----------------
| Grammar rules. |
--------------- */

/* Global program rules. */
program:
          declaration_list
        {
                /* Free block for functions and global variables. */
                top_block = block_free(top_block);
        }
        ;

declaration_list:
          /* empty */
        | declaration_list declaration
        ;

declaration:
          function_declaration ';'
        | function_definition
        ;


/* Function declaration rules. */
function_declaration:
          type IDENTIFIER '(' VOID ')'
        {
                if (sem_function_declaration($2, $1, NULL) != 0) {
                        YYERROR;
                }
        }
        | type IDENTIFIER '(' parameter_type_list ')'
        {
                if (sem_function_declaration($2, $1, $4)  != 0) {
                        YYERROR;
                }
        }
        ;

parameter_type_list:
          data_type
        {
                $$ = var_list_init();
                if ($$ == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }

                if (var_list_push($$, NULL, $1) != 0) { //don't know ID yet
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        | parameter_type_list ',' data_type
        {
                if (var_list_push($1, NULL, $3) != 0) { //don't know ID yet
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        ;


/* Function definition rules. */
function_definition:
          type IDENTIFIER '(' VOID ')' '{'
        { //mid-rule action
                if (sem_function_definition($2, $1, NULL) != 0) {
                        YYERROR;
                }
        } statement_list '}'
        {
                top_block = block_free(top_block); //clear function symbol table
        }
        | type IDENTIFIER '(' parameter_identifier_list ')' '{'
        { //mid-rule action
                if (sem_function_definition($2, $1, $4) != 0) {
                        YYERROR;
                }
        } statement_list '}'
        {
                top_block = block_free(top_block); //clear function symbol table
        }
        ;

parameter_identifier_list:
          data_type IDENTIFIER
        {
                $$ = var_list_init();
                if ($$ == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }

                if (var_list_push($$, $2, $1) != 0) { //push both ID and type
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        | parameter_identifier_list ',' data_type IDENTIFIER
        {
                if (var_list_push($1, $4, $3) != 0) { //push both ID and type
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        ;

/* Statement. */
compound_statement:
          '{'
        { //mid-rule action
                top_block = block_init(top_block);
                if (top_block == NULL) {
                        YYERROR;
                }
        }
          statement_list '}'
        {
                top_block = block_free(top_block); //clear statement symbol tbl
        }
        ;

statement_list:
          /* empty */
        | statement_list statement
        ;

statement:
          variable_definition_statement ';'
        | assignment_statement ';'
        | selection_statement
        | iteration_statement
        | function_call ';'
        | return_statement ';'
 /*       | expression */ /* to tu asi nema byt */
        ;

/* Variable definition statement. */
variable_definition_statement:
          data_type identifier_list
        {
                if (sem_variable_definition_statement($1, $2) != 0) {
                        YYERROR;
                }
        }
        ;

identifier_list:
          IDENTIFIER
        {
                $$ = var_list_init();
                if ($$ == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }

                /* Push only ID, we don't know type yet, put VOID instead. */
                if (var_list_push($$, $1, DATA_TYPE_VOID) != 0) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        | identifier_list ',' IDENTIFIER
        {
                /* Push only ID, we don't know type yet, put VOID instead. */
                if (var_list_push($$, $3, DATA_TYPE_VOID) != 0) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        ;

/* Assignment statement. */
assignment_statement:
          IDENTIFIER '=' expression
        {
                if (block_get(top_block, $1) == NULL) {
                        set_error(RET_SEMANTIC, $1, "used in assignment, but "
                                  "undeclared");
                        YYERROR;
                }
                free($1);
        }
        ;

/* Selection statement. */
selection_statement:
          IF '(' expression ')' compound_statement ELSE compound_statement
        ;

/* Iteration statement. */
iteration_statement:
          WHILE '(' expression ')' compound_statement
        ;

/* Function call statement. */
function_call:
          IDENTIFIER '(' ')'
        {
                if (sem_function_call($1, 0, &$$) != 0) { //put dt into $$
                        YYERROR;
                }
        }
        | IDENTIFIER '(' expression_list ')'
        {
                if (sem_function_call($1, $3, &$$) != 0) { //put dt into $$
                        YYERROR;
                }
        }
        ;

expression_list:
          expression { $$ = 1; }
        | expression_list ',' expression { $$++; }
        ;

/* Return statement. */
return_statement:
          RETURN
        | RETURN expression
        ;


/* Expressions. */
expression:
          /* Literals. */
          INT_LIT
        {
                //printf("t%zu = %d\n", expr_res++, $1);
                $$ = DATA_TYPE_INT;
        }
        | CHAR_LIT
        {
                //printf("t%zu = '%c' (%d)\n", expr_res++, $1, $1);
                $$ = DATA_TYPE_CHAR;
        }
        | STRING_LIT
        {
                //printf("t%zu = %s\n", expr_res++, $1);
                $$ = DATA_TYPE_STRING;
        }

          /* Identifier aka variable. */
        | IDENTIFIER
        {
                //printf("t%zu = %s\n", expr_res++, $1);
                if (sem_expr_identifier($1, &$$) != 0) { //put data type into $$
                        YYERROR;
                }
        }

         /* Parenthesis, cast and function call. */
         /* TODO: associativity */
        | '(' expression ')' { $$ = $2; }
        | '(' data_type ')' expression
        {
                if (sem_expr_cast($2, $4) != 0) {
                        YYERROR;
                }

                $$ = $2; //cast was successful, use new data type
        }
        | function_call { $$ = $1; }

          /* Logical unary negation. */
        | '!' expression %prec NEG
        {
                if ($2 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "!", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }

          /* Integer multiplicative. */
        | expression '*' expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "*", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }
        | expression '/' expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "/", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }
        | expression '%' expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "%", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }

          /* Integer additive. */
        | expression '+' expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "+", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }
        | expression '-' expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "-", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT;
        }

          /* Relation. */
        | expression '<' expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, "<", "incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }
        | expression LE_OP expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, "<=","incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }
        | expression '>' expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, ">", "incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }
        | expression GE_OP expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, ">=","incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }

          /* Comparison. */
        | expression EQ_OP expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, "==","incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }
        | expression NE_OP expression
        {
                if ($1 != $3) {
                        set_error(RET_SEMANTIC, "!=","incompatible data types");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }

          /* Logical AND. */
        | expression AND_OP expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "&&", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }

          /* Logical OP. */
        | expression OR_OP expression
        {
                if ($1 != DATA_TYPE_INT || $3 != DATA_TYPE_INT) {
                        set_error(RET_SEMANTIC, "||", "incompatible data type");
                        YYERROR;
                }

                $$ = DATA_TYPE_INT; //actually 0 or 1
        }
        ;


/* Types and data types. */
data_type:
          INT { $$ = DATA_TYPE_INT; }
        | CHAR { $$ = DATA_TYPE_CHAR; }
        | STRING { $$ = DATA_TYPE_STRING; }
        ;

type:
          VOID { $$ = DATA_TYPE_VOID; }
        | data_type { $$ = $1; }
        ;

%%
/*-----------
| Epilogue. |
-----------*/

/* Error handling definitions. */
static void yyerror (const int *return_code, char const *s)
{
        (void)return_code;

        fprintf(stderr, "%s\n", s);
}

static void set_error(int code, const char *id, const char *message)
{
        print_error(code, id, message);
        *glob_return_code = code;
}


/* Block (chained symbol tables) related definitions. */
static struct block * block_init(struct block *prev)
{
        struct block *block;


        block = malloc(sizeof (struct block));
        if (block == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                return NULL;
        }

        block->symbol_table = ht_init(0);
        if (block->symbol_table == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                free(block);
                return NULL;
        }

        block->prev = prev;


        return block;
}

static struct block * block_free(struct block *block)
{
        struct block *prev;


        assert(block != NULL);

        prev = block->prev;
        ht_free(block->symbol_table, free, free);
        free(block);


        return prev;
}

static void * block_put(struct block *block, const char *id,
                        const struct block_record *br)
{
        const struct block_record *func_br;


        assert(block != NULL && id != NULL);

        /* Check for redefinition in the same scope. */
        if (ht_read(block->symbol_table, id) != NULL) { //ID already defined
                set_error(RET_SEMANTIC, id, "redeclared in the same scope");
                return NULL;
        }

        /* Check if identifier wasn't used for function. */
        func_br = block_get(block, id);
        if (func_br != NULL && func_br->symbol_type == DATA_TYPE_FUNCTION) {
                set_error(RET_SEMANTIC, id, "identifier already used for "
                          "function");
                return NULL;
        }


        return ht_insert(block->symbol_table, id, br); //NULL on memory error
}

static struct block_record * block_get(const struct block *block,
                                       const char *id)
{
        struct block_record *br;


        assert(block != NULL && id != NULL);

        while (block != NULL) { //loop through this and all the previous blocks
                br = ht_read(block->symbol_table, id);
                if (br != NULL) { //ID found
                        return br;
                } else { //ID not found, go lower
                        block = block->prev;
                }
        }


        return NULL; //ID not found
}


/* Semantic actions definitions. */
static int sem_function_declaration(const char *id, data_type_t ret_type,
                                    struct var_list *type_list)
{
        struct block_record *br = malloc(sizeof (struct block_record));


        assert(id != NULL);

        if (br == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                return 1;
        }

        br->symbol_type = DATA_TYPE_FUNCTION;
        br->func_state = FUNC_STATE_DECLARED;
        br->var_list = type_list; //possible NULL for VOID type list
        br->params_cnt = (type_list) ? var_list_get_length(type_list) : 0;
        br->ret_type = ret_type;

        /* Insert record into symbol table. Error on redefinition. */
        if (block_put(top_block, id, br) == NULL) {
                return 1;
        }


        return 0; //success
}

static int sem_function_definition(char *id, data_type_t ret_type,
                                   struct var_list *type_list)
{
        struct block_record *br;
        const char *param_id;
        data_type_t param_type;


        assert(id != NULL);

        br = block_get(top_block, id);
        if (br != NULL) { //ID was already seen
                /* Check if it was seen as a function. */
                if (br->symbol_type != DATA_TYPE_FUNCTION) {
                        set_error(RET_SEMANTIC, id, "was declared as "
                                    "different type");
                        return 1;
                }

                /* Check for function redefinition. */
                if (br->func_state != FUNC_STATE_DECLARED) {
                        set_error(RET_SEMANTIC, id, "already defined");
                        return 1;
                }

                /* Check for type list equality. */
                if (!var_list_are_equal(br->var_list, type_list)) {
                        set_error(RET_SEMANTIC, id, "declaration and "
                                    "definition type list mismatch");
                        return 1;
                }
                /* Declaration variable list is no longer needed. */
                if (br->var_list != NULL) {
                        var_list_free(br->var_list);
                }

                /* Check for return type equality. */
                if (br->ret_type != ret_type) {
                        set_error(RET_SEMANTIC, id, "declaration and "
                                    "definition return type mismatch");
                        return 1;
                }

                free(id); //definition ID is the same as declaration ID
        } else { //ID is new, create new block record
                br = malloc(sizeof (struct block_record));
                if (br == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        return 1;
                }

                /* Insert record into symbol table. Error should not happen. */
                assert(block_put(top_block, id, br) != NULL);
        }

        br->symbol_type = DATA_TYPE_FUNCTION;
        br->func_state = FUNC_STATE_DEFINED;
        br->var_list = type_list; //possible NULL for VOID type list
        br->params_cnt = (type_list) ? var_list_get_length(type_list) : 0;
        br->ret_type = ret_type;


        top_block = block_init(top_block); //now scope for func and its params
        if (top_block == NULL) { //memory exhausted
                return 1;
        }

        if (br->var_list != NULL) { //parameter list is not NULL
                /* Add all params into symbol table for the function scope. */
                param_type = var_list_first(br->var_list, &param_id); //first
                while (param_type != DATA_TYPE_UNSET) {
                        struct block_record *param_br =
                                           malloc(sizeof (struct block_record));

                        if (param_br == NULL) {
                                set_error(RET_INTERNAL, __func__,
                                          "memory exhausted");
                                return 1;
                        }

                        param_br->symbol_type = param_type;
                        if (block_put(top_block, param_id, param_br) == NULL) {
                                return 1; //two parameters of the same ID
                        }

                        param_type = var_list_first(br->var_list, &param_id);
                }

                /* Definition variable list is no longer needed. */
                var_list_free(br->var_list);
        }


        return 0; //success
}

int sem_variable_definition_statement(data_type_t data_type,
                                      struct var_list *id_list)
{
        const char *id;


        assert(id_list != NULL);

        while (var_list_first(id_list, &id) != DATA_TYPE_UNSET) {
                struct block_record *br = malloc(sizeof (struct block_record));

                if (br == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        return 1;
                }

                br->symbol_type = data_type; //same type for the whole list
                if (block_put(top_block, id, br) == NULL) {
                        return 1; //two variables in this scope of the same ID
                }

        }

        /* Variable identifier list is no longer needed. */
        var_list_free(id_list);


        return 0; //success
}

int sem_function_call(char *id, size_t params_cnt, data_type_t *ret_dt)
{
        const struct block_record *br;


        assert(id != NULL);

        br = block_get(top_block, id);
        if (br == NULL) {
                set_error(RET_SEMANTIC, id, "called, but undeclared");
                return 1;
        } else if (br->symbol_type != DATA_TYPE_FUNCTION) {
                set_error(RET_SEMANTIC, id, "called, but not declared as "
                          "function");
                return 1;
        }

        /* TODO: should this be error? */
        if (br->params_cnt > params_cnt) {
                print_warning(RET_SEMANTIC, id, "too few arguments passed");
        } else if (br->params_cnt < params_cnt) {
                print_warning(RET_SEMANTIC, id, "too many arguments passed");
        }

        free(id); //no longer needed
        *ret_dt = br->ret_type;

        //for (size_t i = 0; i < params_cnt; ++i) {
        //        printf("push(t%zu)\n", expr_res - i - 1);
        //}

        return 0; //success
}

int sem_expr_identifier(char *id, data_type_t *data_type)
{
        const struct block_record *br;


        assert(id != NULL);

        br = block_get(top_block, id);
        if (br == NULL) {
                set_error(RET_SEMANTIC, id, "used in expression, but "
                          "undeclared");
                return 1;
        }

        free(id); //no longer needed
        *data_type = br->symbol_type;


        return 0; //success
}

int sem_expr_cast(data_type_t dt_to, data_type_t dt_from)
{
        if (dt_from == dt_to) {
                //cast to the same type, no operation
                return 0; //success
        } else if (dt_from == DATA_TYPE_CHAR && dt_to == DATA_TYPE_STRING) {
                //character to one character long string
                return 0; //success
        } else if (dt_from == DATA_TYPE_CHAR && dt_to == DATA_TYPE_INT) {
                //character's ASCII value to int
                return 0;
        } else if (dt_from == DATA_TYPE_INT && dt_to == DATA_TYPE_CHAR) {
                //integer's LSB to character
                return 0;
        } else {
                set_error(RET_SEMANTIC, NULL, "ilegal cast");
                return 1; //failure
        }
}
