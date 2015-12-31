/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#ifndef COMMON_H
#define COMMON_H


#include <stdio.h>
#include <assert.h>
#include <errno.h>


#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


typedef enum { //program return error codes
        RET_OK = 0,
        RET_LEXICAL = 1,
        RET_SYNTACTIC = 2,
        RET_SEMANTIC = 3,
        RET_GENER = 4,
        RET_INTERNAL = 5,
} return_code_t;


extern return_code_t return_code; //global return code variable


static inline const char *get_prefix(return_code_t code)
{
        switch (code) {
        case RET_LEXICAL:
                return "lexical";
        case RET_SYNTACTIC:
                return "syntactic";
        case RET_SEMANTIC:
                return "semantic";
        case RET_GENER:
                return "code generation";
        case RET_INTERNAL:
                return "internal";
        default:
                assert(!"unknown return code");
        }
}

static inline void print_error(return_code_t code, const char *id,
                const char *message)
{
        if (id) {
                fprintf(stderr, "%s error: \"%s\" %s\n", get_prefix(code), id,
                                message);
        } else {
                fprintf(stderr, "%s error: %s\n", get_prefix(code), message);
        }
}

static inline void print_warning(return_code_t code, const char *id,
                const char *message)
{
        if (id) {
                fprintf(stderr, "%s warning: \"%s\" %s\n", get_prefix(code), id,
                                message);
        } else {
                fprintf(stderr, "%s warning: %s\n", get_prefix(code), message);
        }
}

static inline void set_error(int code, const char *id, const char *message)
{
        print_error(code, id, message);
        return_code = code;
}


#endif //COMMON_H
