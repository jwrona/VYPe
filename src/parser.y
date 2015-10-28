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
        struct hash_table *symbol_table; //symbol table for this block
        struct block *prev; //pointer to previous block
        /*
         * callee_br is pointer to the block record for function, that created
         * the block (and all the sub-block). In level 0 block, that doesn't
         * belong to any function, this is NULL.
         */
        struct block_record *callee_br;
};


int yylex(); //defined in scanner.h

/* Error handling declarations. */
static void yyerror (char const *s);
static void set_error(int code, const char *id, const char *message);

/* Block (chained symbol tables) related declarations. */
static struct block * block_init(struct block *prev,
                                 struct block_record *callee_br);
static struct block * block_free(struct block *block);
static void * block_put(struct block *block, const char *id,
                 const struct block_record *br);
static struct block_record * block_get(const struct block *block,
                                       const char *id);
void block_record_free(struct block_record *br);

/* Semantic actions declarations. */
static int sem_function_declaration(const char *id, data_type_t ret_type,
                              struct var_list *type_list);
static int sem_pre_function_definition(char *id, data_type_t ret_type,
                            struct var_list *type_list);
static int sem_post_function_definition(void);
static int sem_variable_definition_statement(data_type_t data_type,
                                      struct var_list *id_list);
static int sem_assignment_statement(char *id, data_type_t dt);
static int sem_selection_statement(data_type_t dt);
static int sem_iteration_statement(data_type_t dt);
static int sem_function_call(char *id, struct var_list *call_type_list,
                      data_type_t *ret_dt);
static int sem_return_statement(data_type_t data_type);

static int sem_expr_identifier(char *id, data_type_t *data_type);
static int sem_expr_cast(data_type_t dt_to, data_type_t dt_from);


static struct block *top_block = NULL;
static size_t expr_res = 0;

extern return_code_t return_code;
%}

/* pairs with bison-bridge */
%define api.pure
%error-verbose
/* %parse-param {int *return_code} */

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
%type <data_type> expression
%type <data_type> function_call
%type <var_list> parameter_type_list parameter_identifier_list
%type <var_list> identifier_list expression_list

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
        /* Create level 0 block for functions and global variables. */
        top_block = block_init(top_block, NULL);
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
                /* Free level 0 block for functions and global variables. */
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
                /* Initialize list. Push only data type, ID is unkown by now. */
                $$ = var_list_init();
                if ($$ == NULL || var_list_push($$, NULL, $1) != 0) {
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
                if (sem_pre_function_definition($2, $1, NULL) != 0) {
                        YYERROR;
                }
        } statement_list '}'
        {
                if (sem_post_function_definition() != 0) {
                        YYERROR;
                }
        }
        | type IDENTIFIER '(' parameter_identifier_list ')' '{'
        { //mid-rule action
                if (sem_pre_function_definition($2, $1, $4) != 0) {
                        YYERROR;
                }
        } statement_list '}'
        {
                if (sem_post_function_definition() != 0) {
                        YYERROR;
                }
        }
        ;

parameter_identifier_list:
          data_type IDENTIFIER
        {
                /* Initialize list. Push both data type and ID. */
                $$ = var_list_init();
                if ($$ == NULL || var_list_push($$, $2, $1) != 0) {
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
                /*
                 * Create new level > 1 block for function and its parameters.
                 * Inherit callee block record.
                 */
                top_block = block_init(top_block, top_block->callee_br);
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
                /* Push only ID, we don't know type yet, put VOID instead. */
                $$ = var_list_init();
                if ($$ == NULL || var_list_push($$, $1, DATA_TYPE_VOID) != 0) {
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
                if (sem_assignment_statement($1, $3) != 0) {
                        YYERROR;
                }
        }
        ;

/* Selection statement. */
selection_statement:
          IF '(' expression ')' compound_statement ELSE compound_statement
        {
                if (sem_selection_statement($3) != 0) {
                        YYERROR;
                }
        }
        ;

/* Iteration statement. */
iteration_statement:
          WHILE '(' expression ')' compound_statement
        {
                if (sem_iteration_statement($3) != 0) {
                        YYERROR;
                }
        }
        ;

/* Function call statement. */
function_call:
          IDENTIFIER '(' ')'
        {
                if (sem_function_call($1, NULL, &$$) != 0) { //put dt into $$
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
          expression
        {
                /* Initialize list. Push only data type, ID is unkown by now. */
                $$ = var_list_init();
                if ($$ == NULL || var_list_push($$, NULL, $1) != 0) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        | expression_list ',' expression
        {
                if (var_list_push($1, NULL, $3) != 0) { //push type, ID is unk
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        ;

/* Return statement. */
return_statement:
          RETURN
        {
                if (sem_return_statement(DATA_TYPE_VOID) != 0) {
                        YYERROR;
                }
        }
        | RETURN expression
        {
                if (sem_return_statement($2) != 0) {
                        YYERROR;
                }
        }
        ;


/* Expressions. */
expression:
          /* Literals. */
          INT_LIT
        {
                $$ = DATA_TYPE_INT;
        }
        | CHAR_LIT
        {
                $$ = DATA_TYPE_CHAR;
        }
        | STRING_LIT
        {
                $$ = DATA_TYPE_STRING;
                free($1); //XXX
        }

          /* Identifier aka variable. */
        | IDENTIFIER
        {
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
static void yyerror (char const *s)
{
        fprintf(stderr, "%s\n", s);
}

static void set_error(int code, const char *id, const char *message)
{
        print_error(code, id, message);
        return_code = code;
}


/* Block (chained symbol tables) related definitions. */
static struct block * block_init(struct block *prev,
                                 struct block_record *callee_br)
{
        struct block *block;


        block = malloc(sizeof (struct block));
        if (block == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                return NULL;
        }

        block->symbol_table = ht_init(0); //use default buckets count
        if (block->symbol_table == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                free(block);
                return NULL;
        }

        block->prev = prev;
        block->callee_br = callee_br;


        return block;
}

static struct block * block_free(struct block *block)
{
        struct block *prev;


        assert(block != NULL);

        prev = block->prev;
        ht_free(block->symbol_table, free, (void (*)(void *))block_record_free);
        free(block);


        return prev;
}

static void * block_put(struct block *block, const char *id,
                        const struct block_record *br)
{
        const struct block_record *func_br;


        assert(block != NULL && id != NULL);

        /* Check for redefinition in the same block. */
        if (ht_read(block->symbol_table, id) != NULL) { //ID already defined
                set_error(RET_SEMANTIC, id, "redeclared in the same block");
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

void block_record_free(struct block_record *br)
{
        assert(br != NULL);

        /* If symbol was function and had some parameters, free param list. */
        if (br->symbol_type == DATA_TYPE_FUNCTION && br->var_list != NULL) {
                var_list_free(br->var_list);
        }
        free(br);
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
        br->ret_type = ret_type;

        /* Insert record into level 0 block. Error on redefinition. */
        if (block_put(top_block, id, br) == NULL) {
                return 1;
        }


        return 0; //success
}

static int sem_pre_function_definition(char *id, data_type_t ret_type,
                                   struct var_list *type_list)
{
        struct block_record *br;
        const char *param_id;
        data_type_t param_type;


        assert(id != NULL);

        br = block_get(top_block, id); //level 0 block lookup
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
                                    "definition type mismatch");
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

        /* Update record in level 0 block. */
        br->symbol_type = DATA_TYPE_FUNCTION;
        br->func_state = FUNC_STATE_DEFINED;
        br->var_list = type_list; //possible NULL for VOID type list
        br->ret_type = ret_type;


        /* Create new level 1 block for function and its parameters. */
        top_block = block_init(top_block, br);
        if (top_block == NULL) { //memory exhausted
                return 1;
        }

        /* Add all params into symbol table for the function block. */
        param_type = var_list_it_first(br->var_list, &param_id); //first
        while (param_type != DATA_TYPE_UNSET) {
                struct block_record *param_br =
                                           malloc(sizeof (struct block_record));

                if (param_br == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        return 1;
                }

                param_br->symbol_type = param_type;
                if (block_put(top_block, param_id, param_br) == NULL) {
                        return 1; //two parameters of the same ID
                }

                param_type = var_list_it_next(br->var_list, &param_id);
        }


        return 0; //success
}

static int sem_post_function_definition(void)
{
        top_block = block_free(top_block); //clear function symbol table

        return 0;
}

static int sem_variable_definition_statement(data_type_t data_type,
                                      struct var_list *id_list)
{
        data_type_t dt;
        const char *id;


        assert(id_list != NULL);

        /* Loop through all the IDs and put them into symbol table. */
        dt = var_list_it_first(id_list, &id);
        while (dt != DATA_TYPE_UNSET) {
                struct block_record *br = malloc(sizeof (struct block_record));

                if (br == NULL) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        return 1;
                }

                br->symbol_type = data_type; //same type for the whole list
                if (block_put(top_block, id, br) == NULL) {
                        return 1; //two variables in this block of the same ID
                }

                dt = var_list_it_next(id_list, &id);
        }

        /* Variable identifier list is no longer needed. */
        var_list_free(id_list);


        return 0; //success
}

static int sem_assignment_statement(char *id, data_type_t dt)
{
        const struct block_record *br;


        assert(id != NULL);

        br = block_get(top_block, id);
        if (br == NULL) {
                set_error(RET_SEMANTIC, id, "used in assignment, but "
                          "undeclared");
                return 1;
        }

        /* Data types have to be equal. Only int, char and string is allowed. */
        if (br->symbol_type != dt) {
                set_error(RET_SEMANTIC, id, "assignment data type mismatch");
                return 1;
        } else if (dt != DATA_TYPE_INT && dt != DATA_TYPE_CHAR &&
                   dt != DATA_TYPE_STRING) {
                set_error(RET_SEMANTIC, id, "unsupported assignment data type");
                return 1;
        }

        free(id); //no longer needed


        return 0;
}

static int sem_selection_statement(data_type_t dt)
{
        if (dt != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, "if","unsupported contition data type");
                return 1;
        }


        return 0;
}

static int sem_iteration_statement(data_type_t dt)
{
        if (dt != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, "while","unsupported contition data "
                          "type");
                return 1;
        }


        return 0;
}

static int sem_function_call(char *id, struct var_list *call_type_list,
                      data_type_t *ret_dt)
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

        /* Check for type list equality. */
        if (!var_list_are_equal(br->var_list, call_type_list)) {
                set_error(RET_SEMANTIC, id, "declaration/definition and call "
                          "type mismatch");
                return 1;
        }

        free(id); //no longer needed
        if (call_type_list != NULL) {
                var_list_free(call_type_list);
        }

        *ret_dt = br->ret_type;

        //for (size_t i = 0; i < params_cnt; ++i) {
        //        printf("push(t%zu)\n", expr_res - i - 1);
        //}

        return 0; //success
}

static int sem_return_statement(data_type_t data_type)
{
        assert(top_block->callee_br->symbol_type == DATA_TYPE_FUNCTION);

        if (top_block->callee_br->ret_type != data_type) {
                set_error(RET_SEMANTIC, NULL, "return type mismatch");
                return 1;
        }


        return 0;
}


static int sem_expr_identifier(char *id, data_type_t *data_type)
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

static int sem_expr_cast(data_type_t dt_to, data_type_t dt_from)
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
