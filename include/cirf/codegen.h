#ifndef CIRF_CODEGEN_H
#define CIRF_CODEGEN_H

#include "config.h"
#include "error.h"

typedef struct codegen_options {
        const char *name;        /* Base name for generated symbols (e.g., "my_resources") */
        const char *source_path; /* Output .c file path */
        const char *header_path; /* Output .h file path */
} codegen_options_t;

cirf_error_t codegen_generate(const cirf_config_t *config, const codegen_options_t *options);

#endif /* CIRF_CODEGEN_H */
