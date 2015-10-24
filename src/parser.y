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
        data_type_t ret_type;
};

struct block {
        struct hash_table *symbol_table;
        struct block *prev;
};


int yylex(); //defined in scanner.h

static void yyerror (const int *return_code, char const *s);
static void set_error(int code, const char *id, const char *message);

/* Block (chained symbol tables) related declarations. */
static struct block * block_init(struct block *prev);
static struct block * block_free(struct block *block);
static void * block_put(struct block *block, const char *id,
                 const struct block_record *br);
static struct block_record * block_get(const struct block *block,
                                       const char *id);

static int sem_function_declaration(const char *id, data_type_t ret_type,
                              struct var_list *type_list);
static int sem_function_definition(char *id, data_type_t ret_type,
                            struct var_list *type_list);


struct block *top_block = NULL;
int *glob_return_code;
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
%type <var_list> parameter_type_list parameter_identifier_list

/* Operator associativity and precedence. */
%left OR_OP
%left AND_OP
%left EQ_OP NE_OP
%left '<' '>' LE_OP GE_OP
%left '+' '-'
%left '*' '/' '%'
%left NEG /* phony terminal symbol for unary negation '!' */
/*%nonassoc '()' */

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

prog:
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
          function_declaration
        | function_definition
        ;


/* Function declaration rules. */
function_declaration:
          type IDENTIFIER '(' VOID ')' ';'
        {
                if (sem_function_declaration($2, $1, NULL) != 0) {
                        YYERROR;
                }
        }
        | type IDENTIFIER '(' parameter_type_list ')' ';'
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

/******************************************/
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
          variable_definition_statement
        | assignment_statement
        | selection_statement
        | iteration_statement
        | return_statement
        | expression
        ;

variable_definition_statement:
          data_type identifier_list ';'
        ;

identifier_list:
          IDENTIFIER
        {
                free($1);
        }
        | identifier_list ',' IDENTIFIER
        {
                free($3);
        }
        ;

assignment_statement:
          IDENTIFIER '=' expression ';'
        {
                //printf("assignment, \"%s\" ", $1);
                //if (block_get(top_block, $1) != NULL) {
                //        printf("was defined\n");
                //} else {
                //        printf("was not defined\n");
                //}
        }
        ;

selection_statement:
          IF '(' expression ')' compound_statement ELSE compound_statement { printf("selection_statement\n"); }
        ;

iteration_statement:
          WHILE '(' expression ')' compound_statement { printf("iteration_statement\n"); }
        ;

return_statement:
          RETURN ';' { printf("return_statement\n"); }
        | RETURN expression ';' { printf("return_statement\n"); }
        ;

expression:
          BREAK
        ;

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
        //TODO: add free callbacks
        ht_free(block->symbol_table, free, free);
        free(block);


        return prev;
}

static void * block_put(struct block *block, const char *id,
                        const struct block_record *br)
{
        assert(block != NULL && id != NULL);

        if (ht_read(block->symbol_table, id) != NULL) { //id already defined
                set_error(RET_SEMANTIC, id, "redeclared in the same scope");
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
                if (br != NULL) { //id found
                        return br;
                } else { //id not found, go lower
                        block = block->prev;
                }
        }


        return NULL; //id not found
}


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
        br->ret_type = ret_type;

        /* Insert record into symbol table. Error on redefinition. */
        if (block_put(top_block, id, br) == NULL) {
                return 1;
        }


        return 0;
}

static int sem_function_definition(char *id, data_type_t ret_type,
                                   struct var_list *type_list)
{
        struct block_record *br;
        char *param_id;
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

        printf("function %s: \n", id);
                free(id); //definition ID is the same as declaration ID
        } else { //ID is new, create new block record
                br = malloc(sizeof (struct block_record));
                if (br == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        return 1;
                }

                /* Insert recort into symbol table. Error should not happen. */
                assert(block_put(top_block, id, br) != NULL);
        }

        br->symbol_type = DATA_TYPE_FUNCTION;
        br->func_state = FUNC_STATE_DEFINED;
        br->var_list = type_list; //possible NULL for VOID type list
        br->ret_type = ret_type;


        top_block = block_init(top_block); //now scope for func and its params
        if (top_block == NULL) { //memory exhausted
                return 1;
        }

        if (br->var_list != NULL) {
                param_type = var_list_first(br->var_list, &param_id);
                while (param_type != DATA_TYPE_UNSET) {
                        printf("\t%s: %d\n", param_id, param_type);
                        free(param_id);

                        param_type = var_list_first(br->var_list, &param_id);
                }
        }


        /* Definition variable list is no longer needed. */
        if (br->var_list != NULL) {
                var_list_free(br->var_list);
        }


        return 0;
}
