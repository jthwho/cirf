/*
 * cirf/runtime.c - Runtime library implementation
 *
 * Configuration options (define before including or via compiler flags):
 *   CIRF_MAX_PATH     - Maximum path length (default: 256)
 *   CIRF_NO_STDIO     - Disable FILE* functions (for systems without fmemopen)
 *   CIRF_NO_MOUNT     - Disable mount system (saves memory if not needed)
 */

#include "cirf/runtime.h"
#include <string.h>

/* Configurable maximum path length - uses stack allocation */
#ifndef CIRF_MAX_PATH
#define CIRF_MAX_PATH 256
#endif

/* ========================================================================
 * Path-based lookup functions
 * ======================================================================== */

const cirf_file_t *cirf_find_file(const cirf_folder_t *root, const char *path) {
    if(!root || !path) return NULL;

    const char *slash = strrchr(path, '/');
    if(!slash) {
        /* File is in root folder */
        for(size_t i = 0; i < root->file_count; i++) {
            if(strcmp(root->files[i].name, path) == 0) {
                return &root->files[i];
            }
        }
        return NULL;
    }

    /* Check if path matches a folder */
    const cirf_folder_t *folder = cirf_find_folder(root, path);
    if(folder) {
        /* path was a folder, not a file */
        return NULL;
    }

    /* Find parent folder - use stack allocation */
    size_t folder_len = (size_t)(slash - path);
    if(folder_len >= CIRF_MAX_PATH) return NULL;

    char folder_path[CIRF_MAX_PATH];
    memcpy(folder_path, path, folder_len);
    folder_path[folder_len] = '\0';

    folder = cirf_find_folder(root, folder_path);
    if(!folder) return NULL;

    const char *filename = slash + 1;
    for(size_t i = 0; i < folder->file_count; i++) {
        if(strcmp(folder->files[i].name, filename) == 0) {
            return &folder->files[i];
        }
    }
    return NULL;
}

const cirf_folder_t *cirf_find_folder(const cirf_folder_t *root, const char *path) {
    if(!root || !path) return NULL;
    if(*path == '\0') return root;

    const cirf_folder_t *current = root;
    const char          *p = path;

    while(*p && current) {
        /* Skip leading slashes */
        while(*p == '/')
            p++;
        if(*p == '\0') break;

        /* Find end of component */
        const char *end = p;
        while(*end && *end != '/')
            end++;
        size_t len = (size_t)(end - p);

        /* Search for matching child */
        const cirf_folder_t *found = NULL;
        for(size_t i = 0; i < current->child_count; i++) {
            const char *name = current->children[i].name;
            if(strlen(name) == len && memcmp(name, p, len) == 0) {
                found = &current->children[i];
                break;
            }
        }

        current = found;
        p = end;
    }

    return current;
}

/* ========================================================================
 * Metadata functions
 * ======================================================================== */

const char *cirf_get_metadata(const cirf_metadata_t *metadata, size_t count, const char *key) {
    if(!metadata || !key) return NULL;
    for(size_t i = 0; i < count; i++) {
        if(strcmp(metadata[i].key, key) == 0) {
            return metadata[i].value;
        }
    }
    return NULL;
}

/* ========================================================================
 * Navigation functions
 * ======================================================================== */

const cirf_folder_t *cirf_get_root(const cirf_file_t *file) {
    if(!file) return NULL;
    const cirf_folder_t *folder = file->parent;
    while(folder && folder->parent) {
        folder = folder->parent;
    }
    return folder;
}

/* ========================================================================
 * Iteration functions
 * ======================================================================== */

void cirf_foreach_file(const cirf_folder_t *folder, cirf_file_callback_t callback, void *ctx) {
    if(!folder || !callback) return;
    for(size_t i = 0; i < folder->file_count; i++) {
        callback(&folder->files[i], ctx);
    }
}

void cirf_foreach_file_recursive(const cirf_folder_t *folder, cirf_file_callback_t callback,
                                 void *ctx) {
    if(!folder || !callback) return;
    for(size_t i = 0; i < folder->file_count; i++) {
        callback(&folder->files[i], ctx);
    }
    for(size_t i = 0; i < folder->child_count; i++) {
        cirf_foreach_file_recursive(&folder->children[i], callback, ctx);
    }
}

size_t cirf_count_files(const cirf_folder_t *folder) {
    if(!folder) return 0;
    size_t count = folder->file_count;
    for(size_t i = 0; i < folder->child_count; i++) {
        count += cirf_count_files(&folder->children[i]);
    }
    return count;
}

size_t cirf_count_folders(const cirf_folder_t *folder) {
    if(!folder) return 0;
    size_t count = folder->child_count;
    for(size_t i = 0; i < folder->child_count; i++) {
        count += cirf_count_folders(&folder->children[i]);
    }
    return count;
}

/* ========================================================================
 * Standard I/O compatibility (POSIX)
 * ======================================================================== */

#ifndef CIRF_NO_STDIO

/* Check for fmemopen availability */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__) ||                                          \
    (defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L)
#define CIRF_HAVE_FMEMOPEN 1
#endif

#ifdef CIRF_HAVE_FMEMOPEN

FILE *cirf_fopen(const cirf_file_t *file) {
    if(!file) return NULL;
    /* Cast away const - fmemopen with "r" mode won't modify the buffer */
    return fmemopen((void *)file->data, file->size, "r");
}

FILE *cirf_fopen_path(const cirf_folder_t *root, const char *path) {
    const cirf_file_t *file = cirf_find_file(root, path);
    if(!file) return NULL;
    return cirf_fopen(file);
}

#else /* No fmemopen */

FILE *cirf_fopen(const cirf_file_t *file) {
    (void)file;
    return NULL; /* fmemopen not available */
}

FILE *cirf_fopen_path(const cirf_folder_t *root, const char *path) {
    (void)root;
    (void)path;
    return NULL; /* fmemopen not available */
}

#endif /* CIRF_HAVE_FMEMOPEN */

#endif /* CIRF_NO_STDIO */

/* ========================================================================
 * Virtual filesystem mount (optional - requires malloc)
 *
 * Define CIRF_NO_MOUNT to disable this feature for embedded systems
 * that don't need multiple resource sets or want to avoid malloc.
 * ======================================================================== */

#ifndef CIRF_NO_MOUNT

#include <stdlib.h> /* Only needed for mount system */

cirf_mount_t *cirf_mounts = NULL;

int cirf_mount(const char *prefix, const cirf_folder_t *root) {
    if(!prefix || !root) return -1;

    cirf_mount_t *mount = malloc(sizeof(cirf_mount_t));
    if(!mount) return -1;

    mount->prefix = prefix;
    mount->root = root;
    mount->next = cirf_mounts;
    cirf_mounts = mount;
    return 0;
}

int cirf_unmount(const char *prefix) {
    if(!prefix) return -1;

    cirf_mount_t **prev = &cirf_mounts;
    cirf_mount_t  *curr = cirf_mounts;

    while(curr) {
        if(strcmp(curr->prefix, prefix) == 0) {
            *prev = curr->next;
            free(curr);
            return 0;
        }
        prev = &curr->next;
        curr = curr->next;
    }
    return -1;
}

const cirf_file_t *cirf_resolve_file(const char *path) {
    if(!path) return NULL;

    for(cirf_mount_t *m = cirf_mounts; m; m = m->next) {
        size_t prefix_len = strlen(m->prefix);
        if(strncmp(path, m->prefix, prefix_len) == 0) {
            return cirf_find_file(m->root, path + prefix_len);
        }
    }
    return NULL;
}

#ifndef CIRF_NO_STDIO
FILE *cirf_resolve_fopen(const char *path) {
    const cirf_file_t *file = cirf_resolve_file(path);
    if(!file) return NULL;
    return cirf_fopen(file);
}
#endif

#endif /* CIRF_NO_MOUNT */
