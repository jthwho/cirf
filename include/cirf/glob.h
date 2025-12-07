#ifndef CIRF_GLOB_H
#define CIRF_GLOB_H

#include "error.h"

typedef int (*glob_callback_t)(const char *path, void *ctx);

cirf_error_t glob_match(const char *pattern, const char *base_dir, glob_callback_t callback,
                        void *ctx);

int glob_pattern_match(const char *pattern, const char *string);

#endif /* CIRF_GLOB_H */
