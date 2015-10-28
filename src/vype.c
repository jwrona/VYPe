#include "common.h"


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define DEFAULT_OUTPUT_FILE "out.asm"

int yyparse(return_code_t *return_code);
int yylex_destroy(void);
extern FILE *yyin; //defined in scanner.c


return_code_t return_code = RET_OK; //set also by parser


int main(int argc, char **argv)
{
        const char *input_file_name;
        const char *output_file_name;
        int yyret;


        if (argc == 2) {
                input_file_name = argv[1];
                output_file_name = DEFAULT_OUTPUT_FILE;
        } else if (argc == 3) {
                input_file_name = argv[1];
                output_file_name = argv[2];
        } else {
                print_error(RET_INTERNAL, NULL, "bad argument count");
                return RET_INTERNAL;
        }


        yyin = fopen(input_file_name, "r");
        if (yyin == NULL) {
                print_error(RET_INTERNAL, input_file_name, strerror(errno));
                return RET_INTERNAL;
        }

        yyret = yyparse(&return_code);

        if (fclose(yyin) != 0) {
                print_error(RET_INTERNAL, input_file_name, strerror(errno));
        }
        yylex_destroy();


        if (return_code == RET_OK && yyret != 0) {
                return_code = RET_SYNTACTIC;
        }

        if (return_code != RET_OK) {
                fprintf(stderr, "\nexiting with error (%d)\n", return_code);
        }
        return return_code;
}
