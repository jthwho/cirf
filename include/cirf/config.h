#ifndef CIRF_CONFIG_H
#define CIRF_CONFIG_H

#include "error.h"
#include "vfs.h"

typedef struct cirf_config {
    char *name;
    char *base_dir;
    vfs_folder_t *root;
} cirf_config_t;

cirf_error_t config_load(const char *path, const char *name, cirf_config_t **out);
void config_destroy(cirf_config_t *config);

#endif /* CIRF_CONFIG_H */
