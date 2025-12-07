/*
 * cirf/runtime.h - Runtime library for CIRF resources
 *
 * This optional library provides helper functions for navigating and
 * querying CIRF-generated resource trees. Projects that only need direct
 * access to resources via generated symbols do not need this library.
 *
 * Configuration options (define before including):
 *   CIRF_MAX_PATH  - Maximum path length for lookups (default: 256)
 *   CIRF_NO_STDIO  - Disable FILE* functions (cirf_fopen, etc.)
 *   CIRF_NO_MOUNT  - Disable mount system (saves code size, avoids malloc)
 *
 * For embedded systems (ESP32, etc.), you may want:
 *   #define CIRF_NO_STDIO
 *   #define CIRF_NO_MOUNT
 *   #define CIRF_MAX_PATH 128
 *
 * Usage:
 *   #include <cirf/runtime.h>
 *   #include "my_resources.h"  // Your generated resources
 *
 *   const cirf_file_t *file = cirf_find_file(&my_resources_root, "config/app.json");
 */

#ifndef CIRF_RUNTIME_H
#define CIRF_RUNTIME_H

#include "types.h"

#ifndef CIRF_NO_STDIO
#include <stdio.h> /* For FILE*, fmemopen support */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Path-based lookup functions
 * ======================================================================== */

/*
 * Find a file by its virtual path.
 *
 * @param root  Root folder to search from
 * @param path  Virtual path (e.g., "images/icon.png")
 * @return Pointer to file, or NULL if not found
 */
const cirf_file_t *cirf_find_file(const cirf_folder_t *root, const char *path);

/*
 * Find a folder by its virtual path.
 *
 * @param root  Root folder to search from
 * @param path  Virtual path (e.g., "images/icons"), empty string for root
 * @return Pointer to folder, or NULL if not found
 */
const cirf_folder_t *cirf_find_folder(const cirf_folder_t *root, const char *path);

/* ========================================================================
 * Metadata functions
 * ======================================================================== */

/*
 * Get a metadata value by key.
 *
 * @param metadata  Metadata array (from file or folder)
 * @param count     Number of metadata entries
 * @param key       Key to look up
 * @return Value string, or NULL if not found
 */
const char *cirf_get_metadata(const cirf_metadata_t *metadata, size_t count, const char *key);

/* ========================================================================
 * Navigation functions
 * ======================================================================== */

/*
 * Navigate from any file to its root folder.
 *
 * @param file  Any file in the tree
 * @return Root folder, or NULL if file is NULL
 */
const cirf_folder_t *cirf_get_root(const cirf_file_t *file);

/* ========================================================================
 * Iteration functions
 * ======================================================================== */

/*
 * Iterate over all files in a folder (non-recursive).
 *
 * @param folder    Folder to iterate
 * @param callback  Function to call for each file
 * @param ctx       User context passed to callback
 */
void cirf_foreach_file(const cirf_folder_t *folder, cirf_file_callback_t callback, void *ctx);

/*
 * Iterate over all files in a folder tree (recursive).
 *
 * @param folder    Root folder to iterate from
 * @param callback  Function to call for each file
 * @param ctx       User context passed to callback
 */
void cirf_foreach_file_recursive(const cirf_folder_t *folder, cirf_file_callback_t callback,
                                 void *ctx);

/*
 * Count total files in a folder tree (recursive).
 *
 * @param folder  Root folder to count from
 * @return Total number of files
 */
size_t cirf_count_files(const cirf_folder_t *folder);

/*
 * Count total folders in a folder tree (recursive, excluding root).
 *
 * @param folder  Root folder to count from
 * @return Total number of folders (not including the root itself)
 */
size_t cirf_count_folders(const cirf_folder_t *folder);

/* ========================================================================
 * Standard I/O compatibility (POSIX)
 *
 * These functions provide FILE* access to embedded resources, allowing
 * integration with standard C I/O functions and libraries that expect
 * FILE* handles.
 *
 * Note: These require POSIX fmemopen() support. On non-POSIX systems,
 * define CIRF_NO_STDIO to disable these functions.
 * ======================================================================== */

#ifndef CIRF_NO_STDIO

/*
 * Open an embedded file for reading via standard FILE* API.
 *
 * Returns a FILE* that can be used with fread(), fgets(), fseek(), etc.
 * The returned FILE* must be closed with fclose() when done.
 *
 * @param file  Embedded file to open
 * @return FILE* for reading, or NULL on error
 *
 * Example:
 *   const cirf_file_t *f = cirf_find_file(&root, "config.json");
 *   FILE *fp = cirf_fopen(f);
 *   if (fp) {
 *       char buf[256];
 *       while (fgets(buf, sizeof(buf), fp)) {
 *           printf("%s", buf);
 *       }
 *       fclose(fp);
 *   }
 */
FILE *cirf_fopen(const cirf_file_t *file);

/*
 * Open an embedded file by path for reading.
 *
 * Convenience function combining cirf_find_file() and cirf_fopen().
 *
 * @param root  Root folder to search from
 * @param path  Virtual path to the file
 * @return FILE* for reading, or NULL if not found or on error
 */
FILE *cirf_fopen_path(const cirf_folder_t *root, const char *path);

#endif /* CIRF_NO_STDIO */

/* ========================================================================
 * Virtual filesystem mount (advanced)
 *
 * For applications that need to intercept file operations, this provides
 * a mechanism to register CIRF resources under a virtual path prefix.
 *
 * Note: Uses malloc/free. Define CIRF_NO_MOUNT to disable for embedded.
 * ======================================================================== */

#ifndef CIRF_NO_MOUNT

/*
 * Mounted filesystem entry.
 */
typedef struct cirf_mount {
        const char          *prefix; /* Path prefix (e.g., "/assets/") */
        const cirf_folder_t *root;   /* Resource root */
        struct cirf_mount   *next;   /* Next mount in chain */
} cirf_mount_t;

/*
 * Global mount list head (for applications managing multiple resource sets).
 * Applications can maintain their own mount lists instead of using this.
 */
extern cirf_mount_t *cirf_mounts;

/*
 * Mount a resource tree under a path prefix.
 *
 * @param prefix  Path prefix (should end with '/')
 * @param root    Resource root folder
 * @return 0 on success, -1 on error (allocation failure)
 */
int cirf_mount(const char *prefix, const cirf_folder_t *root);

/*
 * Unmount a resource tree.
 *
 * @param prefix  Path prefix to unmount
 * @return 0 on success, -1 if not found
 */
int cirf_unmount(const char *prefix);

/*
 * Find a file across all mounted filesystems.
 *
 * @param path  Full path including mount prefix
 * @return File if found, NULL otherwise
 */
const cirf_file_t *cirf_resolve_file(const char *path);

/*
 * Open a file across all mounted filesystems.
 *
 * @param path  Full path including mount prefix
 * @return FILE* for reading, or NULL if not found
 */
#ifndef CIRF_NO_STDIO
FILE *cirf_resolve_fopen(const char *path);
#endif

#endif /* CIRF_NO_MOUNT */

#ifdef __cplusplus
}
#endif

#endif /* CIRF_RUNTIME_H */
