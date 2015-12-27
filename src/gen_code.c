#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gen_code.h"
#include "reg_alloc.h"

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
		fprintf(f_out, "\tstr%d:\t.asciz\t\"%s\"\n",i,p_lit_strings[i]);
	}
}

void print_vars(FILE * f_out, unsigned n_vars) {
	for (unsigned i = 0; i < n_vars; i++) {
		fprintf(f_out, "\tvar%d:\t.int\t0\n",i);
	}
}

void generate_code(struct tac * tac_mapped, FILE * f_out) {
	// gather data to be declared in the end
	unsigned n_strings = count_string_literals(tac_mapped);
	unsigned n_vars = count_vars(tac_mapped);
	char ** p_lit_strings = malloc(sizeof(char*) * n_strings);
	unsigned i_string = 0;

	// create mappings between variables and registers
	int * var_mapping; int * reg_mapping;
	unsigned res_reg, op1_reg, op2_reg;
	create_variable_mapping(n_vars, &var_mapping);
	create_register_mapping(&reg_mapping);

	// id counter for auxiliary labels
	unsigned generic_label_id = 0;

	// initial settings
	fprintf(f_out,".text\n");
	fprintf(f_out,".org 0\n");
	fprintf(f_out,"li $sp,0x00800000\n");

	// call main and break after it's finished
	// TODO: change to real main call
	fprintf(f_out,"jal label2\n");
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
					// need to do a deep copy
					res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
					op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
					// store adresses of strings
					fprintf(f_out, "\taddi $%d,$28,0\n", res_reg); 
					fprintf(f_out, "\taddi $sp,$sp,-4\n"); 
					fprintf(f_out, "\tsw $%d,0($sp)\n",op1_reg); 
					fprintf(f_out, "\taddi $sp,$sp,-4\n"); 
					fprintf(f_out, "\tsw $%d,0($sp)\n",res_reg); 
					// iterate through strings and do the copy
					fprintf(f_out, "label_copystr%d:\n", generic_label_id); 
					fprintf(f_out, "\tlb $25,0($%d)\n", op1_reg); 
					fprintf(f_out, "\tsb $25,0($%d)\n", res_reg); 
					fprintf(f_out, "\tbeq $25,$0,label_endcopystr%d\n", generic_label_id); 
					fprintf(f_out, "\taddi $%d,$%d,1\n", op1_reg, op1_reg); 
					fprintf(f_out, "\taddi $%d,$%d,1\n", res_reg, res_reg); 
					fprintf(f_out, "\tj label_copystr%d\n",generic_label_id); 
					fprintf(f_out, "label_endcopystr%d:\n", generic_label_id); 
					fprintf(f_out, "\taddi $28,$%d,1\n",res_reg); 
					// restore adresses of strings
					fprintf(f_out, "\tlw $%d,0($sp)\n",res_reg); 
					fprintf(f_out, "\taddi $sp,$sp,4\n"); 
					fprintf(f_out, "\tlw $%d,0($sp)\n",op1_reg); 
					fprintf(f_out, "\taddi $sp,$sp,4\n"); 
					generic_label_id++;
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
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				break;
			case OPERATOR_SLET:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_SGET:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op1_reg, op2_reg);
				fprintf(f_out, "\tlui $25,0xFFFF\n");
				fprintf(f_out, "\tori $25,$25,0xFFFE\n");
				fprintf(f_out, "\tnor $%d,$%d,$25\n", res_reg, res_reg);
				break;
			case OPERATOR_SGT:
				res_reg = get_register(var_mapping, reg_mapping, inst.res_num, inst, f_out);
				op1_reg = get_register(var_mapping, reg_mapping, inst.op1.value.num, inst, f_out);
				op2_reg = get_register(var_mapping, reg_mapping, inst.op2.value.num, inst, f_out);
				fprintf(f_out, "\tslt $%d,$%d,$%d\n", res_reg, op2_reg, op1_reg);
				break;
			case OPERATOR_SE:
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
				break;
			case OPERATOR_CALL:
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
				// print return value
				// TODO: only for debug, remove in the end!
				if (inst.data_type == DATA_TYPE_INT)
					fprintf(f_out,"\tprint_int $2\n");
				else if (inst.data_type == DATA_TYPE_CHAR)
					fprintf(f_out,"\tprint_char $2\n");
				else
					fprintf(f_out,"\tprint_string $2\n");
				fprintf(f_out,"\tli $25,10\n");
				fprintf(f_out,"\tprint_char $25\n");
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

	// dealocate register and variable mappings
	destroy_mappings(reg_mapping, var_mapping);
}
