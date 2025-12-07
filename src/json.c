#include "cirf/json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
        const char *input;
        const char *pos;
        const char *end;
} json_parser_t;

static void skip_whitespace(json_parser_t *p) {
    while(p->pos < p->end && isspace((unsigned char)*p->pos)) {
        p->pos++;
    }
}

static int peek(json_parser_t *p) {
    skip_whitespace(p);
    if(p->pos >= p->end) {
        return -1;
    }
    return (unsigned char)*p->pos;
}

static int consume(json_parser_t *p) {
    skip_whitespace(p);
    if(p->pos >= p->end) {
        return -1;
    }
    return (unsigned char)*p->pos++;
}

static int expect(json_parser_t *p, char c) {
    if(consume(p) != c) {
        return 0;
    }
    return 1;
}

static cirf_error_t parse_value(json_parser_t *p, json_value_t **out);

static cirf_error_t parse_string(json_parser_t *p, char **out) {
    if(!expect(p, '"')) {
        return CIRF_ERR_PARSE;
    }

    const char *start = p->pos;
    size_t      len = 0;
    int         has_escapes = 0;

    while(p->pos < p->end && *p->pos != '"') {
        if(*p->pos == '\\') {
            has_escapes = 1;
            p->pos++;
            if(p->pos >= p->end) {
                return CIRF_ERR_PARSE;
            }
        }
        p->pos++;
        len++;
    }

    if(p->pos >= p->end) {
        return CIRF_ERR_PARSE;
    }

    char *str = malloc(len + 1);
    if(!str) {
        return CIRF_ERR_NOMEM;
    }

    if(has_escapes) {
        const char *src = start;
        char       *dst = str;
        while(src < p->pos) {
            if(*src == '\\') {
                src++;
                switch(*src) {
                    case 'n':
                        *dst++ = '\n';
                        break;
                    case 'r':
                        *dst++ = '\r';
                        break;
                    case 't':
                        *dst++ = '\t';
                        break;
                    case '\\':
                        *dst++ = '\\';
                        break;
                    case '"':
                        *dst++ = '"';
                        break;
                    case '/':
                        *dst++ = '/';
                        break;
                    case 'b':
                        *dst++ = '\b';
                        break;
                    case 'f':
                        *dst++ = '\f';
                        break;
                    case 'u':
                        /* Simplified: just skip \uXXXX and insert ? */
                        src += 4;
                        *dst++ = '?';
                        break;
                    default:
                        *dst++ = *src;
                        break;
                }
                src++;
            } else {
                *dst++ = *src++;
            }
        }
        *dst = '\0';
    } else {
        memcpy(str, start, len);
        str[len] = '\0';
    }

    p->pos++; /* consume closing quote */
    *out = str;
    return CIRF_OK;
}

static cirf_error_t parse_number(json_parser_t *p, long *out) {
    skip_whitespace(p);

    const char *start = p->pos;
    int         negative = 0;

    if(p->pos < p->end && *p->pos == '-') {
        negative = 1;
        p->pos++;
    }

    if(p->pos >= p->end || !isdigit((unsigned char)*p->pos)) {
        p->pos = start;
        return CIRF_ERR_PARSE;
    }

    long value = 0;
    while(p->pos < p->end && isdigit((unsigned char)*p->pos)) {
        value = value * 10 + (*p->pos - '0');
        p->pos++;
    }

    /* Skip fractional part if present */
    if(p->pos < p->end && *p->pos == '.') {
        p->pos++;
        while(p->pos < p->end && isdigit((unsigned char)*p->pos)) {
            p->pos++;
        }
    }

    /* Skip exponent if present */
    if(p->pos < p->end && (*p->pos == 'e' || *p->pos == 'E')) {
        p->pos++;
        if(p->pos < p->end && (*p->pos == '+' || *p->pos == '-')) {
            p->pos++;
        }
        while(p->pos < p->end && isdigit((unsigned char)*p->pos)) {
            p->pos++;
        }
    }

    *out = negative ? -value : value;
    return CIRF_OK;
}

static cirf_error_t parse_array(json_parser_t *p, json_value_t *arr) {
    if(!expect(p, '[')) {
        return CIRF_ERR_PARSE;
    }

    arr->type = JSON_ARRAY;
    arr->data.array.items = NULL;
    arr->data.array.count = 0;

    if(peek(p) == ']') {
        p->pos++;
        return CIRF_OK;
    }

    size_t        capacity = 8;
    json_value_t *items = malloc(capacity * sizeof(json_value_t));
    if(!items) {
        return CIRF_ERR_NOMEM;
    }

    cirf_error_t err;
    size_t       count = 0;

    do {
        if(count >= capacity) {
            capacity *= 2;
            json_value_t *new_items = realloc(items, capacity * sizeof(json_value_t));
            if(!new_items) {
                free(items);
                return CIRF_ERR_NOMEM;
            }
            items = new_items;
        }

        json_value_t *item = NULL;
        err = parse_value(p, &item);
        if(err != CIRF_OK) {
            free(items);
            return err;
        }

        items[count++] = *item;
        free(item);

    } while(peek(p) == ',' && (p->pos++, 1));

    if(!expect(p, ']')) {
        free(items);
        return CIRF_ERR_PARSE;
    }

    arr->data.array.items = items;
    arr->data.array.count = count;
    return CIRF_OK;
}

static cirf_error_t parse_object(json_parser_t *p, json_value_t *obj) {
    if(!expect(p, '{')) {
        return CIRF_ERR_PARSE;
    }

    obj->type = JSON_OBJECT;
    obj->data.object.keys = NULL;
    obj->data.object.values = NULL;
    obj->data.object.count = 0;

    if(peek(p) == '}') {
        p->pos++;
        return CIRF_OK;
    }

    size_t        capacity = 8;
    char        **keys = malloc(capacity * sizeof(char *));
    json_value_t *values = malloc(capacity * sizeof(json_value_t));
    if(!keys || !values) {
        free(keys);
        free(values);
        return CIRF_ERR_NOMEM;
    }

    cirf_error_t err;
    size_t       count = 0;

    do {
        if(count >= capacity) {
            size_t new_capacity = capacity * 2;
            char **new_keys = realloc(keys, new_capacity * sizeof(char *));
            if(!new_keys) {
                for(size_t i = 0; i < count; i++) {
                    free(keys[i]);
                }
                free(keys);
                free(values);
                return CIRF_ERR_NOMEM;
            }
            keys = new_keys;

            json_value_t *new_values = realloc(values, new_capacity * sizeof(json_value_t));
            if(!new_values) {
                for(size_t i = 0; i < count; i++) {
                    free(keys[i]);
                }
                free(keys);
                free(values);
                return CIRF_ERR_NOMEM;
            }
            values = new_values;
            capacity = new_capacity;
        }

        char *key = NULL;
        err = parse_string(p, &key);
        if(err != CIRF_OK) {
            for(size_t i = 0; i < count; i++) {
                free(keys[i]);
            }
            free(keys);
            free(values);
            return err;
        }

        if(!expect(p, ':')) {
            free(key);
            for(size_t i = 0; i < count; i++) {
                free(keys[i]);
            }
            free(keys);
            free(values);
            return CIRF_ERR_PARSE;
        }

        json_value_t *val = NULL;
        err = parse_value(p, &val);
        if(err != CIRF_OK) {
            free(key);
            for(size_t i = 0; i < count; i++) {
                free(keys[i]);
            }
            free(keys);
            free(values);
            return err;
        }

        keys[count] = key;
        values[count] = *val;
        free(val);
        count++;

    } while(peek(p) == ',' && (p->pos++, 1));

    if(!expect(p, '}')) {
        for(size_t i = 0; i < count; i++) {
            free(keys[i]);
        }
        free(keys);
        free(values);
        return CIRF_ERR_PARSE;
    }

    obj->data.object.keys = keys;
    obj->data.object.values = values;
    obj->data.object.count = count;
    return CIRF_OK;
}

static int match_keyword(json_parser_t *p, const char *keyword) {
    skip_whitespace(p);
    size_t len = strlen(keyword);
    if((size_t)(p->end - p->pos) >= len && memcmp(p->pos, keyword, len) == 0) {
        p->pos += len;
        return 1;
    }
    return 0;
}

static cirf_error_t parse_value(json_parser_t *p, json_value_t **out) {
    json_value_t *val = calloc(1, sizeof(json_value_t));
    if(!val) {
        return CIRF_ERR_NOMEM;
    }

    int          c = peek(p);
    cirf_error_t err = CIRF_OK;

    switch(c) {
        case '"':
            val->type = JSON_STRING;
            err = parse_string(p, &val->data.string);
            break;

        case '{':
            err = parse_object(p, val);
            break;

        case '[':
            err = parse_array(p, val);
            break;

        case 't':
            if(match_keyword(p, "true")) {
                val->type = JSON_BOOL;
                val->data.boolean = 1;
            } else {
                err = CIRF_ERR_PARSE;
            }
            break;

        case 'f':
            if(match_keyword(p, "false")) {
                val->type = JSON_BOOL;
                val->data.boolean = 0;
            } else {
                err = CIRF_ERR_PARSE;
            }
            break;

        case 'n':
            if(match_keyword(p, "null")) {
                val->type = JSON_NULL;
            } else {
                err = CIRF_ERR_PARSE;
            }
            break;

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            val->type = JSON_NUMBER;
            err = parse_number(p, &val->data.number);
            break;

        default:
            err = CIRF_ERR_PARSE;
            break;
    }

    if(err != CIRF_OK) {
        free(val);
        return err;
    }

    *out = val;
    return CIRF_OK;
}

cirf_error_t json_parse(const char *input, json_value_t **out) {
    if(!input || !out) {
        return CIRF_ERR_INVALID;
    }

    json_parser_t parser = {.input = input, .pos = input, .end = input + strlen(input)};

    return parse_value(&parser, out);
}

cirf_error_t json_parse_file(const char *path, json_value_t **out) {
    if(!path || !out) {
        return CIRF_ERR_INVALID;
    }

    FILE *fp = fopen(path, "rb");
    if(!fp) {
        return CIRF_ERR_IO;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(size < 0) {
        fclose(fp);
        return CIRF_ERR_IO;
    }

    char *content = malloc((size_t)size + 1);
    if(!content) {
        fclose(fp);
        return CIRF_ERR_NOMEM;
    }

    size_t read = fread(content, 1, (size_t)size, fp);
    fclose(fp);

    if((long)read != size) {
        free(content);
        return CIRF_ERR_IO;
    }

    content[size] = '\0';

    cirf_error_t err = json_parse(content, out);
    free(content);
    return err;
}

static void json_destroy_value(json_value_t *val) {
    if(!val) return;

    switch(val->type) {
        case JSON_STRING:
            free(val->data.string);
            break;

        case JSON_ARRAY:
            for(size_t i = 0; i < val->data.array.count; i++) {
                json_destroy_value(&val->data.array.items[i]);
            }
            free(val->data.array.items);
            break;

        case JSON_OBJECT:
            for(size_t i = 0; i < val->data.object.count; i++) {
                free(val->data.object.keys[i]);
                json_destroy_value(&val->data.object.values[i]);
            }
            free(val->data.object.keys);
            free(val->data.object.values);
            break;

        default:
            break;
    }
}

void json_destroy(json_value_t *value) {
    if(!value) return;
    json_destroy_value(value);
    free(value);
}

json_value_t *json_get(const json_value_t *obj, const char *key) {
    if(!obj || obj->type != JSON_OBJECT || !key) {
        return NULL;
    }

    for(size_t i = 0; i < obj->data.object.count; i++) {
        if(strcmp(obj->data.object.keys[i], key) == 0) {
            return &obj->data.object.values[i];
        }
    }

    return NULL;
}

json_value_t *json_array_get(const json_value_t *arr, size_t index) {
    if(!arr || arr->type != JSON_ARRAY || index >= arr->data.array.count) {
        return NULL;
    }
    return &arr->data.array.items[index];
}

size_t json_array_length(const json_value_t *arr) {
    if(!arr || arr->type != JSON_ARRAY) {
        return 0;
    }
    return arr->data.array.count;
}

size_t json_object_length(const json_value_t *obj) {
    if(!obj || obj->type != JSON_OBJECT) {
        return 0;
    }
    return obj->data.object.count;
}

const char *json_get_string(const json_value_t *obj, const char *key) {
    json_value_t *val = json_get(obj, key);
    if(!val || val->type != JSON_STRING) {
        return NULL;
    }
    return val->data.string;
}

long json_get_number(const json_value_t *obj, const char *key, long default_val) {
    json_value_t *val = json_get(obj, key);
    if(!val || val->type != JSON_NUMBER) {
        return default_val;
    }
    return val->data.number;
}

int json_get_bool(const json_value_t *obj, const char *key, int default_val) {
    json_value_t *val = json_get(obj, key);
    if(!val || val->type != JSON_BOOL) {
        return default_val;
    }
    return val->data.boolean;
}
