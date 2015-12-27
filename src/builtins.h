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
