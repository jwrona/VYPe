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


struct var_list * var_list_init(void);
void var_list_free(struct var_list *vl);
int var_list_push(struct var_list *vl, const char *id,
                data_type_t data_type);
data_type_t var_list_first(struct var_list *vl, const char **id);
int var_list_are_equal(const struct var_list *vl_a,
                const struct var_list *vl_b);
size_t var_list_get_length(const struct var_list *vl);
char * var_list_to_str(const struct var_list *vl);


#endif //DATA_TYPE_H
