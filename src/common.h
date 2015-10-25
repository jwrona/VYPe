#ifndef COMMON_H
#define COMMON_H


#include <stdio.h>
#include <assert.h>
#include <errno.h>


typedef enum { //program return error codes
        RET_OK = 0,
        RET_LEXICAL = 1,
        RET_SYNTACTIC = 2,
        RET_SEMANTIC = 3,
        RET_GENER = 4,
        RET_INTERNAL = 5,
} return_code_t;


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


#endif //COMMON_H
