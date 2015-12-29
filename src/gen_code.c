#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gen_code.h"
#include "reg_alloc.h"

unsigned generic_label_id;

int get_op_val(struct tac_instruction inst, short op) {
	switch (inst.data_type) {
		case DATA_TYPE_INT:
			return (op == 1) ? inst.op1.value.int_val :
					   inst.op2.value.int_val;
		case DATA_TYPE_CHAR:
			return (op == 1) ? inst.op1.value.char_val :
					   inst.op2.value.char_val;
		default:
			return 0;
	}
}

unsigned count_string_literals(struct tac * tac) {
	unsigned n_strings = 0;
	for (unsigned i = 0; i < tac->instructions_cnt; i++) {
		if (tac->instructions[i].operator == OPERATOR_ASSIGN && 
			tac->instructions[i].data_type == DATA_TYPE_STRING &&
				tac->instructions[i].op1.type == OPERAND_TYPE_LITERAL) {
			n_strings++;
		}
		if (tac->instructions[i].operator == OPERATOR_RETURN && 
			tac->instructions[i].data_type == DATA_TYPE_STRING &&
				tac->instructions[i].op1.type == OPERAND_TYPE_LITERAL) {
			n_strings++;
		}
	}
	return n_strings;
}

unsigned count_vars(struct tac * tac) {
	unsigned n_vars = 0;
	for (unsigned i = 0; i < tac->instructions_cnt; i++) {
		struct tac_instruction inst = tac->instructions[i];
		if (inst.op1.type == OPERAND_TYPE_VARIABLE) {
			if (inst.op1.value.num > n_vars) n_vars = inst.op1.value.num;
		}
		if (inst.op2.type == OPERAND_TYPE_VARIABLE) {
			if (inst.op2.value.num > n_vars) n_vars = inst.op2.value.num;
		}
		if (inst.res_num > n_vars) n_vars = inst.res_num;
	}
	return ++n_vars;
}

void print_string_literals(FILE * f_out, char ** p_lit_strings, unsigned n_strings) {
	for (unsigned i = 0; i < n_strings; i++) {
		fprintf(f_out, "\tstr%d:\t.asciz\t\"",i);
		int c = 0;
		while (p_lit_strings[i][c] != 0) {
			if (p_lit_strings[i][c] == '"') fprintf(f_out, "\\\"");	
			else if (p_lit_strings[i][c] == '\\') fprintf(f_out, "\\\\");	
			else fprintf(f_out, "%c", p_lit_strings[i][c]);
			c++;
		}
		fprintf(f_out,"\"\n");
	}
}

void print_vars(FILE * f_out, unsigned n_vars) {
	for (unsigned i = 0; i < n_vars; i++) {
		fprintf(f_out, "\tvar%d:\t.int\t0\n",i);
	}
}

void print_one(int n_param, struct tac * tac, int i_tac, int offset, int * func_params, FILE * f_out) {
	// find the type in tac
	//   go back through tac
	//   if call then ignore func_params[label] pushes before
	//   if push and ignore == 0, we have the type
	int ignore_pushes = 0;
	data_type_t type;
	for (i_tac; ; i_tac--) {
		struct tac_instruction inst = tac->instructions[i_tac];
		if (inst.operator == OPERATOR_CALL) {
			ignore_pushes += func_params[inst.op1.value.num];	
		}
		else if (inst.operator == OPERATOR_PUSH) {
			if (ignore_pushes > 0) {
				ignore_pushes--;
			}
			else {
				type = inst.data_type;
				i_tac--;
				break;
			}
		}
	}

	if (n_param > 0) {
		print_one(n_param - 1, tac, i_tac, offset + 4, func_params, f_out);
	}

	// print the param on SP + offset of type 'type'
	fprintf(f_out, "\tlw $25,%d($sp)\n", offset);
	switch (type) {
        	case DATA_TYPE_INT:
			fprintf(f_out, "\tprint_int $25\n");
			break;
        	case DATA_TYPE_CHAR:
			fprintf(f_out, "\tprint_char $25\n");
			break;
        	case DATA_TYPE_STRING:
			fprintf(f_out, "\tprint_string $25\n");
			break;
		default:
			break;
	}
	
}

void generate_built_in(int code, int n_params, struct tac * tac, int i_tac, 
			int * func_params, FILE * f_out, int * reg_mapping, int * var_mapping) {
	struct tac_instruction inst = tac->instructions[i_tac];
	int res_reg;
	switch (code) {
		case 2: // print
			print_one(n_params-1, tac, i_tac-1, 0, func_params, f_out);	
			fprintf(f_out, "\taddi $sp,$sp,%d\n", n_params*4);
			break;
		case 3: // read char
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			fprintf(f_out, "\tread_char $%d\n", res_reg);
			break;
		case 4: // read int
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			fprintf(f_out, "\tread_int $%d\n", res_reg);
			break;
		case 5: // read string
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			fprintf(f_out, "\taddi $%d,$28,0\n", res_reg);
			fprintf(f_out, "\tread_string $%d,$25\n", res_reg);
			fprintf(f_out, "\tadd $28,$28,$25\n");
			fprintf(f_out, "\tsb $0,0($28)\n");
			fprintf(f_out, "\taddi $28,$28,1\n");
			break;
		case 6: // get_at
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			fprintf(f_out, "\tlw $%d,0($sp)\n",res_reg);
			fprintf(f_out, "\tlw $25,4($sp)\n");
			fprintf(f_out, "\tadd $25,$25,$%d\n", res_reg);
			fprintf(f_out, "\tlb $%d,0($25)\n", res_reg);
			fprintf(f_out, "\taddi $sp,$sp,8\n");
			break;
		case 7: // set_at
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			// store adresses of strings
			fprintf(f_out, "\taddi $%d,$28,0\n", res_reg); 
			fprintf(f_out, "\tlw $28,8($sp)\n"); 
			fprintf(f_out, "\taddi $sp,$sp,-4\n"); 
			fprintf(f_out, "\tsw $%d,0($sp)\n",res_reg); 
			// iterate through strings and do the copy
			fprintf(f_out, "label_copystr%d:\n", generic_label_id); 
			fprintf(f_out, "\tlb $25,0($28)\n"); 
			fprintf(f_out, "\tsb $25,0($%d)\n", res_reg); 
			fprintf(f_out, "\tbeq $25,$0,label_endcopystr%d\n", generic_label_id); 
			fprintf(f_out, "\taddi $28,$28,1\n"); 
			fprintf(f_out, "\taddi $%d,$%d,1\n", res_reg, res_reg); 
			fprintf(f_out, "\tj label_copystr%d\n",generic_label_id); 
			fprintf(f_out, "label_endcopystr%d:\n", generic_label_id); 
			fprintf(f_out, "\taddi $28,$%d,1\n",res_reg); 
			// restore adresses of strings
			fprintf(f_out, "\tlw $%d,0($sp)\n",res_reg); 
			fprintf(f_out, "\taddi $sp,$sp,4\n"); 
			// change the character
			fprintf(f_out, "\tlw $25,4($sp)\n"); 
			fprintf(f_out, "\tadd $%d,$%d,$25\n",res_reg,res_reg); 
			fprintf(f_out, "\tlw $25,0($sp)\n"); 
			fprintf(f_out, "\tsb $25,0($%d)\n", res_reg); 
			fprintf(f_out, "\tlw $25,4($sp)\n"); 
			fprintf(f_out, "\tsub $%d,$%d,$25\n",res_reg,res_reg); 
			fprintf(f_out, "\taddi $sp,$sp,12\n");
			generic_label_id++;
			break;
		case 8: //strcat
			res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
			// store adresses of strings
			fprintf(f_out, "\taddi $%d,$28,0\n", res_reg); 
			fprintf(f_out, "\tlw $28,4($sp)\n"); 
			fprintf(f_out, "\taddi $sp,$sp,-4\n"); 
			fprintf(f_out, "\tsw $%d,0($sp)\n",res_reg); 
			// iterate through strings and do the copy
			fprintf(f_out, "label_copystr%d:\n", generic_label_id); 
			fprintf(f_out, "\tlb $25,0($28)\n"); 
			fprintf(f_out, "\tsb $25,0($%d)\n", res_reg); 
			fprintf(f_out, "\tbeq $25,$0,label_endcopystr%d\n", generic_label_id); 
			fprintf(f_out, "\taddi $28,$28,1\n"); 
			fprintf(f_out, "\taddi $%d,$%d,1\n", res_reg, res_reg); 
			fprintf(f_out, "\tj label_copystr%d\n",generic_label_id); 
			fprintf(f_out, "label_endcopystr%d:\n", generic_label_id); 
			generic_label_id++;
			// store adresses of strings
			fprintf(f_out, "\tlw $28,4($sp)\n"); 
			// iterate through strings and do the copy
			fprintf(f_out, "label_copystr%d:\n", generic_label_id); 
			fprintf(f_out, "\tlb $25,0($28)\n"); 
			fprintf(f_out, "\tsb $25,0($%d)\n", res_reg); 
			fprintf(f_out, "\tbeq $25,$0,label_endcopystr%d\n", generic_label_id); 
			fprintf(f_out, "\taddi $28,$28,1\n"); 
			fprintf(f_out, "\taddi $%d,$%d,1\n", res_reg, res_reg); 
			fprintf(f_out, "\tj label_copystr%d\n",generic_label_id); 
			fprintf(f_out, "label_endcopystr%d:\n", generic_label_id); 
			// restore adresses of strings
			fprintf(f_out, "\taddi $28,$%d,1\n",res_reg);
			fprintf(f_out, "\tlw $%d,0($sp)\n",res_reg); 
			fprintf(f_out, "\taddi $sp,$sp,4\n"); 
			fprintf(f_out, "\taddi $sp,$sp,8\n");
			generic_label_id++;
			break;
	}
}

void count_func_params(struct tac * tac, int ** func_params) {
	unsigned n_labels = 8;
	for (unsigned i = 0; i < tac->instructions_cnt; i++) {
		struct tac_instruction inst = tac->instructions[i];
		if (inst.operator == OPERATOR_LABEL) {
			if (inst.op1.value.num > n_labels) {
				n_labels = inst.op1.value.num;
			}
		}
	}
	n_labels++;

	*func_params = malloc(sizeof(int) * n_labels);
	for (unsigned i = 0; i < n_labels; i++) (*func_params)[i] = 0;
	int curr_label;

	for (unsigned i = 0; i < tac->instructions_cnt; i++) {
		struct tac_instruction inst = tac->instructions[i];
		if (inst.operator == OPERATOR_LABEL) {
			curr_label = inst.op1.value.num;
		}
		else if (inst.operator == OPERATOR_POP) {
			(*func_params)[curr_label]++;
		}
	}

	// n_params for built in functions
	(*func_params)[2] = 0; // print
	(*func_params)[3] = 0; // read_char
	(*func_params)[4] = 0; // read_int
	(*func_params)[5] = 0; // read_string
	(*func_params)[6] = 2; // get_at
	(*func_params)[7] = 3; // set_at
	(*func_params)[8] = 2; // strcat
}

void compare_strings(struct tac_instruction inst, int * var_mapping, int * reg_mapping,
			FILE * f_out, operator_t operator) {
	int res_reg, op1_reg, op2_reg;
	res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
	op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
	op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);

	fprintf(f_out, "\taddi $sp,$sp,-4\n");
	fprintf(f_out, "\tsw $%d,0($sp)\n", op1_reg);
	fprintf(f_out, "\taddi $sp,$sp,-4\n");
	fprintf(f_out, "\tsw $%d,0($sp)\n", op2_reg);

	fprintf(f_out, "label_compstr%d:\n", generic_label_id);
	fprintf(f_out, "\tlb $25,0($%d)\n", op1_reg);
	fprintf(f_out, "\tlb $%d,0($%d)\n", res_reg, op2_reg);
	fprintf(f_out, "\tsub $25,$25,$%d\n", res_reg);
	fprintf(f_out, "\tbne $25,$0,label_compstr_end%d\n", generic_label_id);
	fprintf(f_out, "\tbeq $%d,$0,label_compstr_end%d\n", res_reg, generic_label_id);
	fprintf(f_out, "\taddi $%d,$%d,1\n", op1_reg, op1_reg);
	fprintf(f_out, "\taddi $%d,$%d,1\n", op2_reg, op2_reg);
	fprintf(f_out, "\tj label_compstr%d\n", generic_label_id);
	fprintf(f_out, "label_compstr_end%d:\n", generic_label_id);
	fprintf(f_out, "\tadd $25,$25,$%d\n", res_reg);

	fprintf(f_out, "\taddi $%d,$%d,0\n", op2_reg, res_reg);
	fprintf(f_out, "\taddi $%d,$25,0\n", op1_reg);

	switch (operator) {
		case OPERATOR_SLT:
			fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
			break;
		case OPERATOR_SLET:
			fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
			fprintf(f_out, "\tlui $25,0xFFFF\n");
			fprintf(f_out, "\tori $25,$25,0xFFFE\n");
			fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
			break;
		case OPERATOR_SGET:
			fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
			fprintf(f_out, "\tlui $25,0xFFFF\n");
			fprintf(f_out, "\tori $25,$25,0xFFFE\n");
			fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
			break;
		case OPERATOR_SGT:
			fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
			break;
		case OPERATOR_SE:
			fprintf(f_out, "\tsub $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
			fprintf(f_out, "\tsltu $%d,$zero,$%d\n", res_reg, res_reg);
			fprintf(f_out, "\tlui $25,0xFFFF\n");
			fprintf(f_out, "\tori $25,$25,0xFFFE\n");
			fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
			break;
		case OPERATOR_SNE:
			fprintf(f_out, "\tsub $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
			fprintf(f_out, "\tsltu $%d,$zero,$%d\n", res_reg, res_reg);
			break;
		default:
			break;
	}

	fprintf(f_out, "\tlw $%d,0($sp)\n", op2_reg);
	fprintf(f_out, "\taddi $sp,$sp,4\n");
	fprintf(f_out, "\tlw $%d,0($sp)\n", op1_reg);
	fprintf(f_out, "\taddi $sp,$sp,4\n");

	generic_label_id++;

}

void generate_code(struct tac * tac_mapped, FILE * f_out) {
	// gather data to be declared in the end
	unsigned n_strings = count_string_literals(tac_mapped);
	unsigned n_vars = count_vars(tac_mapped);
	char ** p_lit_strings = malloc(sizeof(char*) * n_strings);
	unsigned i_string = 0;

	int * func_params;
	count_func_params(tac_mapped, &func_params);
	int n_pushes = 0;

	// create mappings between variables and registers
	int * var_mapping; int * reg_mapping;
	unsigned res_reg, op1_reg, op2_reg;
	create_variable_mapping(n_vars, &var_mapping);
	create_register_mapping(&reg_mapping);

	// id counter for auxiliary labels
	generic_label_id = 0;

	// initial settings
	fprintf(f_out,".text\n");
	fprintf(f_out,".org 0\n");
	fprintf(f_out,"li $sp,0x00800000\n");
	fprintf(f_out,"la $28,heap\n");

	// call main and break after it's finished
	fprintf(f_out,"jal label1\n");
	fprintf(f_out,"break\n");	

	for (unsigned i = 0; i < tac_mapped->instructions_cnt; i++) {
		struct tac_instruction inst = tac_mapped->instructions[i];
		switch (inst.operator) {
			case OPERATOR_LABEL:
				clear_mappings(var_mapping, n_vars, reg_mapping, f_out);
				fprintf(f_out, "\nlabel%d:\n",inst.op1.value.num);
				break;
			case OPERATOR_ASSIGN:
				if ((inst.data_type == DATA_TYPE_STRING) && 
				    (inst.op1.type == OPERAND_TYPE_LITERAL)) { //string literal
					p_lit_strings[i_string] = inst.op1.value.string_val;
					res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
					fprintf(f_out, "\tla $%d,str%d\n", res_reg, i_string);
					i_string++;
				}
				else if (inst.data_type == DATA_TYPE_STRING) { //string
					// not doing deep copy, because we cannot change the string anyway
					res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
					op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
					fprintf(f_out, "\taddi $%d,$%d,0\n", res_reg, op1_reg);
				}
				else if (inst.op1.type == OPERAND_TYPE_LITERAL) { // int or char literal
					res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
					fprintf(f_out, "\tli $%d,%d\n", res_reg, get_op_val(inst, 1));
				}
				else { // int or char
					res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
					op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
					fprintf(f_out, "\taddi $%d,$%d,0\n", res_reg, op1_reg);
				}	
				break;
			case OPERATOR_SLT:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SLT);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				break;
			case OPERATOR_SLET:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SLET);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_SGET:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SGET);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_SGT:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SGT);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
				break;
			case OPERATOR_SE:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SE);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tsub $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				fprintf(f_out, "\tsltu $%d,$zero,$%d\n", res_reg, res_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_SNE:
				if (inst.data_type == DATA_TYPE_STRING) {
					compare_strings(inst, var_mapping, reg_mapping, f_out, OPERATOR_SNE);
					break;
				}
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tsub $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				fprintf(f_out, "\tsltu $%d,$zero,$%d\n", res_reg, res_reg);
				break;
			case OPERATOR_BZERO:
				clear_mappings(var_mapping, n_vars, reg_mapping, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out, "\tbeq $%d, $0, label%d\n", op1_reg, inst.op2.value.num);
				break;
			case OPERATOR_NEG:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out, "\tsltu $%d,$zero,$%d\n", res_reg, op1_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_AND:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\taddi $%d,$zero,0\n", res_reg);
				fprintf(f_out, "\tbeq $%d, $0, labelgen%d\n", op1_reg, generic_label_id);
				fprintf(f_out, "\tbeq $%d, $0, labelgen%d\n", op2_reg, generic_label_id);
				fprintf(f_out, "\taddi $%d,$zero,1\n", res_reg);
				fprintf(f_out, "\nlabelgen%d:\n",generic_label_id);
				generic_label_id++;
				break;
			case OPERATOR_OR:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\taddi $%d,$zero,1\n", res_reg);
				fprintf(f_out, "\tbne $%d, $0, labelgen%d\n", op1_reg, generic_label_id);
				fprintf(f_out, "\tbne $%d, $0, labelgen%d\n", op2_reg, generic_label_id);
				fprintf(f_out, "\taddi $%d,$zero,0\n", res_reg);
				fprintf(f_out, "\nlabelgen%d:\n",generic_label_id);
				generic_label_id++;
				break;
			case OPERATOR_JUMP:
				clear_mappings(var_mapping, n_vars, reg_mapping, f_out);
				fprintf(f_out, "\tj label%d\n",inst.op1.value.num);
				break;
			case OPERATOR_SUB:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tsub $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				break;
			case OPERATOR_ADD:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tadd $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				break;
			case OPERATOR_DIV:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tdiv $%d,$%d\n", op1_reg, op2_reg);
				fprintf(f_out, "\tmflo $%d\n", res_reg);
				break;
			case OPERATOR_MOD:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tdiv $%d,$%d\n", op1_reg, op2_reg);
				fprintf(f_out, "\tmfhi $%d\n", res_reg);
				break;
			case OPERATOR_MUL:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tmul $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				break;
			case OPERATOR_POP:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				fprintf(f_out, "\tlw $%d,0($fp)\n", res_reg);
				fprintf(f_out,"\taddi $fp,$fp,4\n");
				break;
			case OPERATOR_PUSH:
				// push param on stack
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out,"\taddi $sp,$sp,-4\n");
				fprintf(f_out,"\tsw $%d,0($sp)\n",op1_reg);
				n_pushes++;
				break;
			case OPERATOR_CALL:
				n_pushes -= func_params[inst.op1.value.num];

				if ((inst.op1.value.num >=2) && inst.op1.value.num <= 8) {
					// built in function
					generate_built_in(inst.op1.value.num, n_pushes, tac_mapped, i, 
								func_params, f_out, reg_mapping, var_mapping);
					if (inst.op1.value.num == 2) n_pushes = 0;
					break;
				}
				// push old FP
				fprintf(f_out,"\taddi $sp,$sp,-4\n");
				fprintf(f_out,"\tsw $fp,0($sp)\n");
				// set new FP 
				fprintf(f_out,"\taddi $fp,$sp,4\n");
				// push old RA
				fprintf(f_out,"\taddi $sp,$sp,-4\n");
				fprintf(f_out,"\tsw $ra,0($sp)\n");
				// push vars
				clear_mappings(var_mapping, n_vars, reg_mapping, f_out);
				fprintf(f_out,"\tjal push_registers\n");
				// call
				fprintf(f_out,"\tjal label%d\n",inst.op1.value.num);
				// pop vars
				clear_mappings(var_mapping, n_vars, reg_mapping, f_out);
				fprintf(f_out,"\tjal pop_registers\n");
				// pop ra
				fprintf(f_out,"\tlw $ra,0($sp)\n");
				fprintf(f_out,"\taddi $sp,$sp,4\n");
				// pop fp + params	
				fprintf(f_out,"\taddi $25,$fp,0\n");
				fprintf(f_out,"\tlw $fp,0($sp)\n");
				fprintf(f_out,"\taddi $sp,$25,0\n");
				// save return value
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				fprintf(f_out,"\taddi $%d,$2,0\n",res_reg);
				break;
			case OPERATOR_RETURN:
				if (inst.op1.type == OPERAND_TYPE_LITERAL) {
					if (inst.data_type == DATA_TYPE_STRING) {
						p_lit_strings[i_string] = inst.op1.value.string_val;
						fprintf(f_out, "\tla $2,str%d\n", i_string);
						i_string++;
					}
					else {
						fprintf(f_out,"\tli $2,%d\n",get_op_val(inst,1));
						fprintf(f_out, "\tjr $ra\n");
					}
				}
				else {
					op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
					fprintf(f_out,"\taddi $2,$%d,0\n",op1_reg);
					fprintf(f_out, "\tjr $ra\n");
				}
				break;
			case OPERATOR_CAST_INT_TO_CHAR:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out, "\tli $%d,0\n",res_reg);
				fprintf(f_out, "\taddi $%d,$%d,0\n",res_reg,op1_reg);
				fprintf(f_out, "\tli $25,0x00FF\n");
				fprintf(f_out, "\tand $%d,$%d,$25\n",res_reg,res_reg);
				break;
			case OPERATOR_CAST_CHAR_TO_INT:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out, "\tli $%d,0\n",res_reg);
				fprintf(f_out, "\taddi $%d,$%d,0\n",res_reg,op1_reg);
				//fprintf(f_out, "\tli $25,0x000F\n");
				//fprintf(f_out, "\tand $%d,$%d,$25\n",res_reg,res_reg);
				break;
			case OPERATOR_CAST_CHAR_TO_STRING:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				fprintf(f_out, "\taddi $%d,$28,0\n",res_reg);
				fprintf(f_out, "\tsb $%d,0($%d)\n",op1_reg,res_reg);
				fprintf(f_out, "\tsb $0,1($%d)\n",res_reg);
				fprintf(f_out, "\taddi $28,$28,2\n");
				break;
			default:
				fprintf(f_out, "\tMISSING INSTR\n");
				break;
		}
	}

	// generate push_registers function
	fprintf(f_out,"\npush_registers:\n");
	for (unsigned i_reg = 0; i_reg < n_vars; i_reg++) {
		fprintf(f_out,"\taddi $sp,$sp,-4\n");
		fprintf(f_out,"\tla $25,var%d\n",i_reg);
		fprintf(f_out,"\tlw $8,0($25)\n");
		fprintf(f_out,"\tsw $8,0($sp)\n");
	}
	fprintf(f_out,"\tjr $ra\n");

	// generate pop_registers function
	fprintf(f_out,"\npop_registers:\n");
	for (int i_reg = n_vars-1; i_reg >= 0; i_reg--) {
		fprintf(f_out,"\tla $25,var%d\n",i_reg);
		fprintf(f_out,"\tlw $8,0($sp)\n");
		fprintf(f_out,"\tsw $8,0($25)\n");
		fprintf(f_out,"\taddi $sp,$sp,4\n");
	}
	fprintf(f_out,"\tjr $ra\n");

	// print data - strings + variables
	fprintf(f_out,"\n.data\n");
	print_string_literals(f_out, p_lit_strings, n_strings);
	print_vars(f_out, n_vars);
	fprintf(f_out,"\n.align 4\n");
	fprintf(f_out,"\nheap:\n");

	free(func_params);

	// dealocate register and variable mappings
	destroy_mappings(reg_mapping, var_mapping);
}
