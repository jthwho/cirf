#ifndef CIRF_JSON_H
#define CIRF_JSON_H

#include "error.h"
#include <stddef.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;

struct json_value {
        json_type_t type;
        union {
                int   boolean;
                long  number;
                char *string;
                struct {
                        json_value_t *items;
                        size_t        count;
                } array;
                struct {
                        char        **keys;
                        json_value_t *values;
                        size_t        count;
                } object;
        } data;
};

cirf_error_t json_parse(const char *input, json_value_t **out);
cirf_error_t json_parse_file(const char *path, json_value_t **out);
void         json_destroy(json_value_t *value);

json_value_t *json_get(const json_value_t *obj, const char *key);
json_value_t *json_array_get(const json_value_t *arr, size_t index);
size_t        json_array_length(const json_value_t *arr);
size_t        json_object_length(const json_value_t *obj);

const char *json_get_string(const json_value_t *obj, const char *key);
long        json_get_number(const json_value_t *obj, const char *key, long default_val);
int         json_get_bool(const json_value_t *obj, const char *key, int default_val);

#endif /* CIRF_JSON_H */
