#include "tac.h"
#include "common.h"

#include <stdlib.h>

#define TAC_INIT_SIZE 128

struct tac { //three address code structure
        struct tac_instruction *instructions; //array of instructions
        size_t instructions_cnt; //number of instructions in the array

        size_t size; //actual array size
};


const char *operator_str[] = {
        "", //unary operators
        "ASSIGN",
        "NEG",
        "CAST_INT_TO_CHAR",
        "CAST_CHAR_TO_INT",
        "CAST_CHAR_TO_STRING",

        "", //binary operators
        /* Additive and multiplicative. */
        "ADD",
        "SUB",
        "MUL",
        "DIV",
        "MOD",

        /* Relation. */
        "SE",
        "SNE",
        "SLT",
        "SLET",
        "SGT",
        "SGET",

        /* Logical. */
        "AND",
        "OR",
};

const char *operator_symbol[] = {
        "", //unary operators
        "=",
        "!",
        "CAST_INT_TO_CHAR",
        "CAST_CHAR_TO_INT",
        "CAST_CHAR_TO_STRING",

        "", //binary operators
        /* Additive and multiplicative. */
        "+",
        "-",
        "*",
        "/",
        "%",

        /* Relation. */
        "==",
        "!=",
        "<",
        "<=",
        ">",
        ">=",

        /* Logical. */
        "&&",
        "||",
};


static int tac_resize(struct tac *tac)
{
        assert(tac != NULL);

        struct tac_instruction *new_instructions;
        const size_t new_size = (tac->size == 0)? TAC_INIT_SIZE : tac->size * 2;

        new_instructions = realloc(tac->instructions,
                        new_size * sizeof (struct tac_instruction));
        if (new_instructions == NULL) {
                set_error(RET_INTERNAL, __func__, "memory exhausted");
                return 1;
        }

        tac->instructions = new_instructions;
        tac->size = new_size;

        return 0;
}

static void tac_operand_print(const struct tac_operand op,
                data_type_t data_type)
{
        if (op.type == OPERAND_TYPE_VARIABLE) {
                printf("t%u", op.value.var_num);
        } else {
                switch (data_type) {
                case DATA_TYPE_INT:
                        printf("%d", op.value.int_val);
                        break;
                case DATA_TYPE_CHAR:
                        printf("'%c'", op.value.char_val);
                        break;
                case DATA_TYPE_STRING:
                        printf("\"%s\"", op.value.string_val);
                        break;
                default:
                        assert(!"bad operand data type");
                }
        }
}


struct tac * tac_init(void)
{
        return calloc(1, sizeof (struct tac));
}

void tac_free(struct tac *tac)
{
        assert(tac != NULL);

        /* Free memory allocated in scanner for string literals. */
        for (size_t i = 0; i < tac->instructions_cnt; ++i) {
                struct tac_instruction instr = tac->instructions[i];

                if (instr.data_type == DATA_TYPE_STRING) {
                        if (instr.op1.type == OPERAND_TYPE_LITERAL) {
                                free(instr.op1.value.string_val);
                        }
                        if (instr.op2.type == OPERAND_TYPE_LITERAL) {
                                free(instr.op2.value.string_val);
                        }
                }
        }

        free(tac->instructions);
        free(tac);
}

int tac_add(struct tac *tac, struct tac_instruction instruction)
{
        assert(tac != NULL);

        if (tac->instructions_cnt == tac->size) { //array full, inflate it
                if (tac_resize(tac) != 0) {
                        return 1;
                }
        }

        tac->instructions[tac->instructions_cnt++] = instruction;

        return 0;
}

void tac_print(struct tac *tac)
{
        assert(tac != NULL);

        for (size_t i = 0; i < tac->instructions_cnt; ++i) {
                struct tac_instruction instr = tac->instructions[i];

                printf("t%u = ", instr.res_num);
                tac_operand_print(instr.op1, instr.data_type);
                printf(" %s ", operator_str[instr.operator]);

                if (instr.operator > _OPERATOR_BINARY) {
                        tac_operand_print(instr.op2, instr.data_type);
                }

                printf("\t%s\n", data_type_str[instr.data_type]);
        }
}
