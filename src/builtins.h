/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#ifndef BUILTINS_H
#define BUILTINS_H


#include "data_type.h"


struct function {
        const char *id;
        unsigned tac_num;
        data_type_t ret_type;
        const data_type_t *params;
        size_t params_cnt;
};


#endif //BUILTINS_H
