%{
/* Prologue. */
#include "common.h"
#include "data_type.h"
#include "hash_table.h"
#include "tac.h"

#include <stdio.h>
#include <assert.h>


typedef enum {
        FUNC_STATE_UNSET,
        FUNC_STATE_DECLARED,
        FUNC_STATE_DEFINED,
} func_state_t;

struct block_record {
        data_type_t symbol_type; //int, char, string or function
        unsigned tac_num; //number assigned in three address code

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
static int sem_assignment_statement(char *id, struct block_record expr_br);
static int sem_selection_statement(struct block_record expr_br);
static int sem_iteration_statement(struct block_record expr_br);
static int sem_function_call(char *id, struct var_list *call_type_list,
                             struct block_record *ret_br);
static int sem_return_statement(struct block_record expr_br);

static int sem_expr_literal(data_type_t data_type, void *data,
                             struct block_record *res_br);
static int sem_expr_identifier(char *id, struct block_record *res_br);
static int sem_expr_cast(data_type_t dt_to, struct block_record expr_br,
                         struct block_record *res_br);
static int sem_expr_integer_unary(struct block_record op,
                                  struct block_record *res_br,
                                  operator_t operator);
static int sem_expr_integer_binary(struct block_record op1,
                                   struct block_record op2,
                                   struct block_record *res_br,
                                   operator_t operator);
static int sem_expr_relation(struct block_record op1, struct block_record op2,
                             struct block_record *res_br, operator_t operator);


static struct block *top_block = NULL; //pointer to current block
static struct tac_instruction instr; //three address code instruction
static unsigned tac_cntr = 0; //three address code result counter

extern struct tac *tac; //three address code
%}

/* pairs with bison-bridge */
%define api.pure
%error-verbose

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
        struct block_record block_record;
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
%type <block_record> expression function_call
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
                if (sem_function_call($1, NULL, &$$) != 0) {
                        YYERROR;
                }
        }
        | IDENTIFIER '(' expression_list ')'
        {
                if (sem_function_call($1, $3, &$$) != 0) {
                        YYERROR;
                }
        }
        ;

expression_list:
          expression
        {
                /* Initialize list. Push only data type, ID is unkown by now. */
                $$ = var_list_init();
                if ($$ == NULL || var_list_push($$, NULL, $1.symbol_type) != 0){
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        | expression_list ',' expression
        {
                if (var_list_push($1, NULL, $3.symbol_type) != 0) {
                        set_error(RET_INTERNAL, __func__, "memory exhausted");
                        YYERROR;
                }
        }
        ;

/* Return statement. */
return_statement:
          RETURN
        {
                struct block_record br = {
                        .symbol_type = DATA_TYPE_VOID
                };

                if (sem_return_statement(br) != 0) {
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
                if (sem_expr_literal(DATA_TYPE_INT, &$1, &$$) != 0) {
                        YYERROR;
                }
        }
        | CHAR_LIT
        {
                if (sem_expr_literal(DATA_TYPE_CHAR, &$1, &$$) != 0) {
                        YYERROR;
                }
        }
        | STRING_LIT
        {
                if (sem_expr_literal(DATA_TYPE_STRING, $1, &$$) != 0) {
                        YYERROR;
                }
        }

          /* Identifier aka variable. */
        | IDENTIFIER
        {
                if (sem_expr_identifier($1, &$$) != 0) { //put block_rec into $$
                        YYERROR;
                }
        }

         /* Parenthesis, cast and function call. */
         /* TODO: associativity */
        | '(' expression ')' { $$ = $2; /* copy block_record further */ }
        | '(' data_type ')' expression
        {
                if (sem_expr_cast($2, $4, &$$) != 0) { //put block_rec into $$
                        YYERROR;
                }
        }
        | function_call { $$ = $1; /* copy block_record further */ }

          /* Logical unary negation. */
        | '!' expression %prec NEG
        {
                if (sem_expr_integer_unary($2, &$$, OPERATOR_NEG) != 0) {
                        YYERROR;
                }
        }

          /* Integer multiplicative. */
        | expression '*' expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_MUL) != 0) {
                        YYERROR;
                }
        }
        | expression '/' expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_DIV) != 0) {
                        YYERROR;
                }
        }
        | expression '%' expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_MOD) != 0) {
                        YYERROR;
                }
        }

          /* Integer additive. */
        | expression '+' expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_ADD) != 0) {
                        YYERROR;
                }
        }
        | expression '-' expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_SUB) != 0) {
                        YYERROR;
                }
        }

          /* Relation. */
        | expression '<' expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SLT) != 0) {
                        YYERROR;
                }
        }
        | expression LE_OP expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SLET) != 0) {
                        YYERROR;
                }
        }
        | expression '>' expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SGT) != 0) {
                        YYERROR;
                }
        }
        | expression GE_OP expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SGET) != 0) {
                        YYERROR;
                }
        }
        | expression EQ_OP expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SE) != 0) {
                        YYERROR;
                }
        }
        | expression NE_OP expression
        {
                if (sem_expr_relation($1, $3, &$$, OPERATOR_SNE) != 0) {
                        YYERROR;
                }
        }

          /* Logical AND. */
        | expression AND_OP expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_AND) != 0) {
                        YYERROR;
                }
        }

          /* Logical OP. */
        | expression OR_OP expression
        {
                if (sem_expr_integer_binary($1, $3, &$$, OPERATOR_OR) != 0) {
                        YYERROR;
                }
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
                param_br->tac_num = tac_cntr++; //assign unique TAC number
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
                br->tac_num = tac_cntr++; //assign unique TAC number
                if (block_put(top_block, id, br) == NULL) {
                        return 1; //two variables in this block of the same ID
                }

                dt = var_list_it_next(id_list, &id);
        }

        /* Variable identifier list is no longer needed. */
        var_list_free(id_list);


        return 0; //success
}

static int sem_assignment_statement(char *id, struct block_record expr_br)
{
        const struct block_record *id_br; //fetched block record for ID


        assert(id != NULL);

        id_br = block_get(top_block, id);
        if (id_br == NULL) {
                set_error(RET_SEMANTIC, id, "used in assignment, but "
                          "undeclared");
                return 1;
        }

        /* Data types have to be equal. Only int, char and string is allowed. */
        if (expr_br.symbol_type != id_br->symbol_type) {
                set_error(RET_SEMANTIC, id, "assignment data type mismatch");
                return 1;
        } else if (id_br->symbol_type != DATA_TYPE_INT &&
                   id_br->symbol_type != DATA_TYPE_CHAR &&
                   id_br->symbol_type != DATA_TYPE_STRING) {
                set_error(RET_SEMANTIC, id, "unsupported assignment data type");
                return 1;
        }

        free(id); //no longer needed


        /* Generate TAC. */
        instr.data_type = id_br->symbol_type;
        instr.res_num = id_br->tac_num;
        instr.operator = OPERATOR_ASSIGN; //unary

        instr.op1.type = OPERAND_TYPE_VARIABLE;
        instr.op1.value.var_num = expr_br.tac_num;


        return tac_add(tac, instr); //success or memory exhaustion
}

static int sem_selection_statement(struct block_record expr_br)
{
        if (expr_br.symbol_type != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, "if","unsupported contition data type");
                return 1;
        }


        return 0; //success
}

static int sem_iteration_statement(struct block_record expr_br)
{
        if (expr_br.symbol_type != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, "while","unsupported contition data "
                          "type");
                return 1;
        }


        return 0; //success
}

static int sem_function_call(char *id, struct var_list *call_type_list,
                             struct block_record *ret_br)
{
        const struct block_record *id_br;


        assert(id != NULL && ret_br != NULL);

        id_br = block_get(top_block, id);
        if (id_br == NULL) {
                set_error(RET_SEMANTIC, id, "called, but undeclared");
                return 1;
        } else if (id_br->symbol_type != DATA_TYPE_FUNCTION) {
                set_error(RET_SEMANTIC, id, "called, but not declared as "
                          "function");
                return 1;
        }

        /* Check for type list equality. */
        if (!var_list_are_equal(id_br->var_list, call_type_list)) {
                set_error(RET_SEMANTIC, id, "declaration/definition and call "
                          "type mismatch");
                return 1;
        }

        free(id); //no longer needed
        if (call_type_list != NULL) {
                var_list_free(call_type_list);
        }

        ret_br->symbol_type = id_br->ret_type;
        ret_br->tac_num = tac_cntr++;

        //for (size_t i = 0; i < params_cnt; ++i) {
        //        printf("push(t%zu)\n", expr_res - i - 1);
        //}

        return 0; //success
}

static int sem_return_statement(struct block_record expr_br)
{
        assert(top_block->callee_br->symbol_type == DATA_TYPE_FUNCTION);

        if (top_block->callee_br->ret_type != expr_br.symbol_type) {
                set_error(RET_SEMANTIC, NULL, "return type mismatch");
                return 1;
        }


        return 0;
}


static int sem_expr_literal(data_type_t data_type, void *data,
                             struct block_record *res_br)
{
        res_br->symbol_type = data_type;
        res_br->tac_num = tac_cntr++;

        /* Generate TAC. */
        instr.data_type = res_br->symbol_type;
        instr.res_num = res_br->tac_num;
        instr.operator = OPERATOR_ASSIGN; //unary

        instr.op1.type = OPERAND_TYPE_LITERAL;

        switch (data_type) {
        case DATA_TYPE_INT:
                instr.op1.value.int_val = *(int *)data;
                break;
        case DATA_TYPE_CHAR:
                instr.op1.value.char_val = *(char *)data;
                break;
        case DATA_TYPE_STRING:
                instr.op1.value.string_val = (char *)data;
                break;
        default:
                assert(!"bad literal data type");
        }


        return tac_add(tac, instr); //success or memory exhaustion
}

static int sem_expr_identifier(char *id, struct block_record *expr_br)
{
        const struct block_record *id_br;


        assert(id != NULL && expr_br != NULL);

        id_br = block_get(top_block, id);
        if (id_br == NULL) {
                set_error(RET_SEMANTIC, id, "used in expression, but "
                          "undeclared");
                return 1;
        }

        free(id); //no longer needed
        *expr_br = *id_br; //copy ID block record into expression block record


        return 0; //success
}

static int sem_expr_cast(data_type_t dt_to, struct block_record expr_br,
                         struct block_record *res_br)
{
        assert(res_br != NULL);

        if (expr_br.symbol_type == dt_to) {
                //cast to the same type, no operation
        } else if (expr_br.symbol_type == DATA_TYPE_CHAR &&
                   dt_to == DATA_TYPE_STRING) {
                /* Character to one character long string. */
                instr.operator = OPERATOR_CAST_CHAR_TO_STRING; //unary
        } else if (expr_br.symbol_type == DATA_TYPE_CHAR &&
                   dt_to == DATA_TYPE_INT) {
                /* Character's ASCII value to integer. */
                instr.operator = OPERATOR_CAST_CHAR_TO_INT; //unary
        } else if (expr_br.symbol_type == DATA_TYPE_INT &&
                   dt_to == DATA_TYPE_CHAR) {
                /* Integer's LSB to character. */
                instr.operator = OPERATOR_CAST_INT_TO_CHAR; //unary
        } else {
                set_error(RET_SEMANTIC, NULL, "ilegal cast");
                return 1; //failure
        }

        *res_br = expr_br; //copy block_record further, but
        res_br->symbol_type = dt_to; //change symbol type to desired type
        res_br->tac_num = tac_cntr++;

        /* Generate TAC. */
        instr.data_type = expr_br.symbol_type; //put source data type into TAC
        instr.res_num = res_br->tac_num;

        instr.op1.type = OPERAND_TYPE_VARIABLE;
        instr.op1.value.var_num = expr_br.tac_num;


        return tac_add(tac, instr); //success or memory exhaustion
}

static int sem_expr_integer_unary(struct block_record op,
                                  struct block_record *res_br,
                                  operator_t operator)
{
        assert(res_br != NULL);

        if (op.symbol_type != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, operator_symbol[operator],
                          "incompatible data type");
                return 1;
        }


        res_br->symbol_type = DATA_TYPE_INT;
        res_br->tac_num = tac_cntr++;

        /* Generate TAC. */
        instr.data_type = res_br->symbol_type;
        instr.res_num = res_br->tac_num;
        instr.operator = operator; //unary

        instr.op1.type = OPERAND_TYPE_VARIABLE;
        instr.op1.value.var_num = op.tac_num;


        return tac_add(tac, instr); //success or memory exhaustion
}

static int sem_expr_integer_binary(struct block_record op1,
                                   struct block_record op2,
                                   struct block_record *res_br,
                                   operator_t operator)
{
        assert(res_br != NULL);

        if (op1.symbol_type != DATA_TYPE_INT ||
            op2.symbol_type != DATA_TYPE_INT) {
                set_error(RET_SEMANTIC, operator_symbol[operator],
                          "incompatible data type");
                return 1;
        }


        res_br->symbol_type = DATA_TYPE_INT; //0 or 1 only for logical AND OR
        res_br->tac_num = tac_cntr++;

        /* Generate TAC. */
        instr.data_type = res_br->symbol_type;
        instr.res_num = res_br->tac_num;
        instr.operator = operator; //binary

        instr.op1.type = OPERAND_TYPE_VARIABLE;
        instr.op1.value.var_num = op1.tac_num;

        instr.op2.type = OPERAND_TYPE_VARIABLE;
        instr.op2.value.var_num = op2.tac_num;


        return tac_add(tac, instr); //success or memory exhaustion
}

static int sem_expr_relation(struct block_record op1, struct block_record op2,
                             struct block_record *res_br, operator_t operator)
{
        assert(res_br != NULL);

        if (op1.symbol_type != op2.symbol_type) {
                set_error(RET_SEMANTIC, operator_symbol[operator],
                          "incompatible data types");
                return 1;
        }

        res_br->symbol_type = DATA_TYPE_INT; //actually 0 or 1
        res_br->tac_num = tac_cntr++;

        /* Generate TAC. */
        instr.data_type = op1.symbol_type;
        instr.res_num = res_br->tac_num;
        instr.operator = operator; //binary

        instr.op1.type = OPERAND_TYPE_VARIABLE;
        instr.op1.value.var_num = op1.tac_num;

        instr.op2.type = OPERAND_TYPE_VARIABLE;
        instr.op2.value.var_num = op2.tac_num;


        return tac_add(tac, instr); //success or memory exhaustion
}
