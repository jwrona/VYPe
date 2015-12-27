#include "builtins.h"
#include "common.h"


static const data_type_t get_at_params[] = {DATA_TYPE_STRING, DATA_TYPE_INT};
static const data_type_t set_at_params[] = {DATA_TYPE_STRING, DATA_TYPE_INT,
        DATA_TYPE_CHAR};
static const data_type_t strcat_params[] = {DATA_TYPE_STRING, DATA_TYPE_STRING};


const struct function builtins[] = {
        {"print", 2, DATA_TYPE_VOID, NULL, 0},
        {"read_char", 3, DATA_TYPE_CHAR, NULL, 0},
        {"read_int", 4, DATA_TYPE_INT, NULL, 0},
        {"read_string", 5, DATA_TYPE_STRING, NULL, 0},
        {"get_at", 6, DATA_TYPE_CHAR, get_at_params, ARRAY_SIZE(get_at_params)},
        {"set_at", 7, DATA_TYPE_STRING, set_at_params,
                ARRAY_SIZE(set_at_params)},
        {"strcat", 8, DATA_TYPE_STRING, strcat_params,
                ARRAY_SIZE(strcat_params)},
};
const size_t builtins_cnt = ARRAY_SIZE(builtins);
