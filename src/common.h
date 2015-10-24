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


static inline void print_error(return_code_t code, const char *id,
                const char *message)
{
        const char *prefix;


        switch (code) {
        case RET_LEXICAL:
                prefix = "lexical";
                break;
        case RET_SYNTACTIC:
                prefix = "syntactic";
                break;
        case RET_SEMANTIC:
                prefix = "semantic";
                break;
        case RET_GENER:
                prefix = "code generation";
                break;
        case RET_INTERNAL:
                prefix = "internal";
                break;
        default:
                assert(!"unknown return code");
        }

        if (id) {
                fprintf(stderr, "%s error: \"%s\" %s\n", prefix, id, message);
        } else {
                fprintf(stderr, "%s error: %s\n", prefix, message);
        }
}


#endif //COMMON_H
