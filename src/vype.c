/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#include "common.h"
#include "tac.h"
#include "gen_code.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>


#define DEFAULT_OUTPUT_FILE "out.asm"

int yyparse(return_code_t *return_code);
int yylex_destroy(void);
extern FILE *yyin; //defined in scanner.c


return_code_t return_code = RET_OK; //set also by parser
struct tac *tac;


int main(int argc, char **argv)
{
        const char *input_file_name;
        const char *output_file_name;
        int yyret;


        /* Handle command line arguments. */
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


        /* Initialize TAC and open input file. */
        tac = tac_init();
        if (tac == NULL) {
                print_error(RET_INTERNAL, __func__, "memory exhausted");
                return RET_INTERNAL;
        }

        yyin = fopen(input_file_name, "r");
        if (yyin == NULL) {
                print_error(RET_INTERNAL, input_file_name, strerror(errno));
                return RET_INTERNAL;
        }

        /* Parsing, semantic checks and TAC generation. */
        yyret = yyparse(&return_code);

        if (fclose(yyin) != 0) {
                print_error(RET_INTERNAL, input_file_name, strerror(errno));
        }
        yylex_destroy();


        if (return_code == RET_OK && yyret == 0) { //parsing was successfull
                //tac_print(tac);
                FILE * fout = fopen(output_file_name, "w");
                if (fout == NULL) {
                        print_error(RET_INTERNAL, output_file_name,
                                        strerror(errno));
                        return RET_INTERNAL;
                }

                generate_code(tac, fout);
                if (fclose(fout) != 0) {
                        print_error(RET_INTERNAL, output_file_name,
                                        strerror(errno));
                }
        }

        tac_free(tac);


        if (return_code == RET_OK && yyret != 0) {
                return_code = RET_SYNTACTIC;
        }

        if (return_code != RET_OK) {
                fprintf(stderr, "\nexiting with error (%d)\n", return_code);
        }
        return return_code;
}
