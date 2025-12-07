#include "cirf/error.h"

#define CIRF_ERROR_STRING(name, str) str,

static const char *error_strings[] = {
    CIRF_ERROR_LIST(CIRF_ERROR_STRING)
};

#undef CIRF_ERROR_STRING

const char *cirf_error_string(cirf_error_t err)
{
    if (err < 0 || err >= CIRF_ERROR_COUNT) {
        return "unknown error";
    }
    return error_strings[err];
}
