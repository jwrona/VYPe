/*
 * project: VYPe15 programming language compiler
 * author: Jan Wrona <xwrona00@stud.fit.vutbr.cz>
 * author: Katerina Zmolikova <xzmoli02@stud.fit.vutbr.cz>
 * date: 2015
 */
#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#include <stdlib.h> //size_t

typedef enum {
        DATA_TYPE_UNSET,
        DATA_TYPE_VOID,
        DATA_TYPE_INT,
        DATA_TYPE_CHAR,
        DATA_TYPE_STRING,
        DATA_TYPE_FUNCTION,
} data_type_t;

extern const char *data_type_str[];

struct var_list * var_list_init(void);
void var_list_free(struct var_list *vl);

int var_list_push(struct var_list *vl, const char *id,
                data_type_t data_type);

data_type_t var_list_it_first(struct var_list *vl, const char **id);
data_type_t var_list_it_last(struct var_list *vl, const char **id);
data_type_t var_list_it_next(struct var_list *vl, const char **id);
data_type_t var_list_it_prev(struct var_list *vl, const char **id);

int var_list_are_equal(const struct var_list *vl_a,
                const struct var_list *vl_b);


#endif //DATA_TYPE_H
