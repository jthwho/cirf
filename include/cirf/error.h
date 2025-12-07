#ifndef CIRF_ERROR_H
#define CIRF_ERROR_H

#define CIRF_ERROR_LIST(X) \
    X(CIRF_OK,            "success") \
    X(CIRF_ERR_NOMEM,     "out of memory") \
    X(CIRF_ERR_IO,        "I/O error") \
    X(CIRF_ERR_PARSE,     "parse error") \
    X(CIRF_ERR_INVALID,   "invalid argument") \
    X(CIRF_ERR_NOT_FOUND, "not found") \
    X(CIRF_ERR_DUPLICATE, "duplicate entry")

#define CIRF_ERROR_ENUM(name, str) name,

typedef enum {
    CIRF_ERROR_LIST(CIRF_ERROR_ENUM)
    CIRF_ERROR_COUNT
} cirf_error_t;

#undef CIRF_ERROR_ENUM

const char *cirf_error_string(cirf_error_t err);

#endif /* CIRF_ERROR_H */
