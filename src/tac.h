#ifndef TAC_H
#define TAC_H


#include "data_type.h"


typedef enum {
        OPERATOR_UNSET, //special value

        _OPERATOR_NULLARY,
        OPERATOR_POP, //stack pop

        _OPERATOR_UNARY,
        OPERATOR_ASSIGN,  //res = op1
        OPERATOR_NEG,  //res = !op1
        OPERATOR_CAST_INT_TO_CHAR, //integer's LSB to character
        OPERATOR_CAST_CHAR_TO_INT, //character's ASCII value to int
        OPERATOR_CAST_CHAR_TO_STRING, //character to one character long string

        OPERATOR_LABEL, //label
        OPERATOR_JUMP, //unconditional jump, goto op1
        OPERATOR_CALL, //function call
        OPERATOR_RETURN, //return from function call
        OPERATOR_PUSH, //stack push

        _OPERATOR_BINARY,
        /* Additive and multiplicative. */
        OPERATOR_ADD, //res = op1 + op2
        OPERATOR_SUB, //res = op1 - op2
        OPERATOR_MUL, //res = op1 * op2
        OPERATOR_DIV, //res = op1 / op2
        OPERATOR_MOD, //res = op1 % op2

        /* Relation. */
        OPERATOR_SE,   //res = (op1 == op2) ? 1 : 0 (set on equal)
        OPERATOR_SNE,  //res = (op1 != op2) ? 1 : 0 (set on not equal)
        OPERATOR_SLT,  //res = (op1 < op2)  ? 1 : 0 (less than)
        OPERATOR_SLET, //res = (op1 <= op2) ? 1 : 0 (less or equal than)
        OPERATOR_SGT,  //res = (op1 > op2)  ? 1 : 0 (greater than)
        OPERATOR_SGET, //res = (op1 <= op2) ? 1 : 0 (greater or equal than)

        /* Logical. */
        OPERATOR_AND, //res = op1 && op2
        OPERATOR_OR,  //res = op1 || op2

        OPERATOR_BZERO, //if (op1 == 0) goto op2
} operator_t;

typedef enum {
        OPERAND_TYPE_UNUSED,
        OPERAND_TYPE_VARIABLE,
        OPERAND_TYPE_LABEL,
        OPERAND_TYPE_LITERAL,
} operand_type_t;

struct tac_operand {
        operand_type_t type; //variable or literal
        union {
                unsigned num; //variable/label number
                int int_val; //integer literal value
                char char_val; //character literal value
                char *string_val; //string literal value
        } value;
};

struct tac_instruction {
        data_type_t data_type; //no impicit conversion (int, char or string)

        unsigned res_num; //result number
        operator_t operator;
        struct tac_operand op1;
        struct tac_operand op2;
};

extern const char *operator_str[];
extern const char *operator_symbol[];

struct tac * tac_init(void);
void tac_free(struct tac *tac);
int tac_add(struct tac *tac, struct tac_instruction instruction);
void tac_print(struct tac *tac);


#endif //TAC_H
