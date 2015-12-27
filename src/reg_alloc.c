#include "stdlib.h"
#include "stdio.h"
#include "reg_alloc.h"

const int n_registers = 24-8+1;
int free_reg = 8;
int dump_reg = 8;

void create_register_mapping(int ** mapping) {
	*mapping = malloc(24 * sizeof(int));
	for (int i = 0; i < 24; i++) {
		(*mapping)[i] = -1;
	}
}

void create_variable_mapping(int n_vars, int ** mapping) {
	*mapping = malloc(n_vars * sizeof(int));
	for (int i = 0; i < n_vars; i++) {
		(*mapping)[i] = -1;
	}
}

void destroy_mappings(int * var_mapping, int * reg_mapping) {
	free(var_mapping);
	free(reg_mapping);
}

int get_free_register(int * var_mapping, int * reg_mapping, struct tac_instruction inst, FILE * f_out) {
	if (free_reg <= 24) {
		return free_reg++;
	}
	else {
		int dump_var = reg_mapping[dump_reg];
		while ((dump_var == (int)inst.res_num) || 
			((inst.op1.type == OPERAND_TYPE_VARIABLE) && (dump_var == (int)inst.op1.value.num)) ||
			((inst.op2.type == OPERAND_TYPE_VARIABLE) && (dump_var == (int)inst.op2.value.num))) {
			dump_reg = dump_reg + 1;
			if (dump_reg == 25) dump_reg = 8;
			dump_var = reg_mapping[dump_reg];
		}
		int reg = dump_reg;
			
		fprintf(f_out,"\tla $25,var%d\n",dump_var);
		fprintf(f_out, "\tsw $%d,0($25)\n",reg);
		var_mapping[dump_var] = -1;
		dump_reg = dump_reg + 1;
		if (dump_reg == 25) dump_reg = 8;
		return reg;
	}
}

int get_register(int * var_mapping, int * reg_mapping, int var, struct tac_instruction inst, FILE * f_out) {
	if (var_mapping[var] != -1) {
		return var_mapping[var];
	}
	int reg = get_free_register(var_mapping, reg_mapping, inst, f_out);
	// load var to register
	fprintf(f_out,"\tla $25,var%d\n",var);
	fprintf(f_out, "\tlw $%d,0($25)\n",reg);
	// update mappings
	var_mapping[var] = reg;
	reg_mapping[reg] = var;
	return reg;
}

void clear_mappings(int * var_mapping, int n_vars, int * reg_mapping, FILE * f_out) {
	for (int i = 0; i < n_vars; i++) {
		if (var_mapping[i] != -1) {
			fprintf(f_out,"\tla $25,var%d\n",i);
			fprintf(f_out, "\tsw $%d,0($25)\n",var_mapping[i]);
			var_mapping[i] = -1;
		}		
	}
	for (int i = 0; i < n_registers; i++) {
		reg_mapping[i] = -1;
	}
	free_reg = 8; dump_reg = 8;
	
}
