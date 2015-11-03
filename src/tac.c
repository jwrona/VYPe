#include "tac.h"
#include "common.h"

#include <stdlib.h>
#include <string.h>

#define TAC_INIT_SIZE 128

struct tac { //three address code structure
        struct tac_instruction *instructions; //array of instructions
        size_t instructions_cnt; //number of instructions in the array

        size_t size; //actual array size
};


const char *operator_str[] = {
        "---UNSET---",

        "", //nullary operators
        "POP",

        "", //unary operators
        "ASSIGN",
        "NEG",
        "CAST_INT_TO_CHAR",
        "CAST_CHAR_TO_INT",
        "CAST_CHAR_TO_STRING",

        "LABEL",
        "JUMP",
        "CALL",
        "RETURN",
        "PUSH",

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

        "BZERO",
};

const char *operator_symbol[] = {
        "---UNSET---",

        "", //nullary operators
        "POP",

        "", //unary operators
        "=",
        "!",
        "CAST_INT_TO_CHAR",
        "CAST_CHAR_TO_INT",
        "CAST_CHAR_TO_STRING",

        "LABEL",
        "JUMP",
        "CALL",
        "RETURN",
        "PUSH",

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

        "BZERO",
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
        int off = 0;


        switch (op.type) {
        case OPERAND_TYPE_UNUSED:
                putchar('-');
                off++;
                break;

        case OPERAND_TYPE_VARIABLE:
                if (op.value.num != 0) {
                        off += printf("var: %u", op.value.num);
                } else {
                        off += printf("-");
                }
                break;

        case OPERAND_TYPE_LABEL:
                off += printf("lbl: %u", op.value.num);
                break;

        case OPERAND_TYPE_LITERAL:
                switch (data_type) {
                case DATA_TYPE_INT:
                        off += printf("lit: %d", op.value.int_val);
                        break;
                case DATA_TYPE_CHAR:
                        off += printf("lit: '%c'", op.value.char_val);
                        break;
                case DATA_TYPE_STRING:
                        off += printf("lit: \"%s\"", op.value.string_val);
                        break;
                default:
                        assert(!"bad operand data type");
                }
                break;

        default:
                assert(!"bad operand type");
        }

        off = 30 - off;
        if (off < 5) {
                off = 5;
        }

        printf("%*s", off, "");
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

        printf("%-*s", 25, "instruction");
        printf("%-*s", 8, "result");
        printf("%-*s", 30, "operand 1");
        printf("%-*s", 30, "operand 2");
        printf("%s", "data type");
        putchar('\n');
        for (size_t i = 0; i < 102; ++i) {
                putchar('-');
        }
        putchar('\n');

        for (const struct tac_instruction *instr = tac->instructions;
                        instr < tac->instructions + tac->instructions_cnt;
                        ++instr)
        {
                if (instr->operator == OPERATOR_LABEL) {
                        printf("%-*s", 25, operator_str[instr->operator]);
                } else {
                        printf("  %-*s", 23, operator_str[instr->operator]);
                }
                if (instr->res_num != 0) {
                        printf("%-*u", 8, instr->res_num);
                } else {
                        printf("-%-*s", 7, "");
                }
                tac_operand_print(instr->op1, instr->data_type);
                tac_operand_print(instr->op2, instr->data_type);
                if (instr->data_type != DATA_TYPE_UNSET) {
                        printf("%s", data_type_str[instr->data_type]);
                } else {
                        putchar('-');
                }
                putchar('\n');
        }
}
