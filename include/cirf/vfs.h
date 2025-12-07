#ifndef CIRF_VFS_H
#define CIRF_VFS_H

#include "error.h"
#include <stddef.h>

typedef struct vfs_metadata {
    char *key;
    char *value;
    struct vfs_metadata *next;
} vfs_metadata_t;

typedef struct vfs_file {
    char *name;
    char *path;
    char *source_path;
    char *mime;
    unsigned char *data;
    size_t size;
    vfs_metadata_t *metadata;
    struct vfs_folder *parent;
    struct vfs_file *next;
} vfs_file_t;

typedef struct vfs_folder {
    char *name;
    char *path;
    vfs_metadata_t *metadata;
    struct vfs_folder *parent;
    struct vfs_folder *children;
    struct vfs_folder *next;
    vfs_file_t *files;
} vfs_folder_t;

vfs_folder_t *vfs_create_root(void);
void vfs_destroy(vfs_folder_t *root);

vfs_folder_t *vfs_add_folder(vfs_folder_t *parent, const char *name);
vfs_folder_t *vfs_find_folder(vfs_folder_t *root, const char *path);
vfs_folder_t *vfs_ensure_folder(vfs_folder_t *root, const char *path);

vfs_file_t *vfs_add_file(vfs_folder_t *parent, const char *name,
                          const char *source_path);
vfs_file_t *vfs_find_file(vfs_folder_t *root, const char *path);

cirf_error_t vfs_load_file_data(vfs_file_t *file);
cirf_error_t vfs_load_all_data(vfs_folder_t *root);

void vfs_add_metadata(vfs_metadata_t **list, const char *key, const char *value);
const char *vfs_get_metadata(const vfs_metadata_t *list, const char *key);
size_t vfs_metadata_count(const vfs_metadata_t *list);

size_t vfs_folder_count(const vfs_folder_t *folder);
size_t vfs_file_count(const vfs_folder_t *folder);

#endif /* CIRF_VFS_H */
