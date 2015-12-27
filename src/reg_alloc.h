#include "tac.h"

void create_register_mapping(int ** mapping);
void create_variable_mapping(int n_vars, int ** mapping);
int get_register(int * var_mapping, int * reg_mapping, int var, struct tac_instruction inst, FILE * f_out);
void clear_mappings(int * var_mapping, int n_vars, int * reg_mapping, FILE * f_out);
void destroy_mappings(int * var_mapping, int * reg_mapping);
