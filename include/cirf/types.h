/*
 * cirf/types.h - Common type definitions for CIRF generated resources
 *
 * This header defines the standard structures used by all CIRF-generated
 * resource files. Include this header to work with CIRF resources from
 * multiple source files or to use the cirf_runtime library.
 */

#ifndef CIRF_TYPES_H
#define CIRF_TYPES_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Metadata key-value pair.
 */
typedef struct cirf_metadata {
    const char *key;
    const char *value;
} cirf_metadata_t;

/*
 * Forward declaration for folder type.
 */
typedef struct cirf_folder cirf_folder_t;

/*
 * Embedded file entry.
 */
typedef struct cirf_file {
    const char *name;              /* Filename only (e.g., "icon.png") */
    const char *path;              /* Full virtual path (e.g., "images/icon.png") */
    const char *mime;              /* MIME type (e.g., "image/png") */
    const unsigned char *data;     /* Raw file data */
    size_t size;                   /* File size in bytes */
    const cirf_folder_t *parent;   /* Parent folder */
    const cirf_metadata_t *metadata;
    size_t metadata_count;
} cirf_file_t;

/*
 * Virtual folder/directory.
 */
struct cirf_folder {
    const char *name;              /* Folder name only (e.g., "images") */
    const char *path;              /* Full virtual path (e.g., "assets/images") */
    const cirf_folder_t *parent;   /* Parent folder (NULL for root) */
    const cirf_folder_t *children; /* First child folder (array) */
    size_t child_count;            /* Number of child folders */
    const cirf_file_t *files;      /* Files in this folder (array) */
    size_t file_count;             /* Number of files */
    const cirf_metadata_t *metadata;
    size_t metadata_count;
};

/*
 * Callback type for file iteration.
 */
typedef void (*cirf_file_callback_t)(const cirf_file_t *file, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CIRF_TYPES_H */
