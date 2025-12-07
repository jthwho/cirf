/*
 * CIRF Simple Example
 *
 * This example demonstrates both:
 * 1. Direct symbol access (works without cirf_runtime library)
 * 2. Runtime library functions (requires linking cirf_runtime)
 */

#include <stdio.h>
#include "simple_resources.h"
#include <cirf/runtime.h>  /* For cirf_find_file, cirf_fopen, etc. */

static void print_file(const cirf_file_t *file, void *ctx)
{
    (void)ctx;
    printf("  %s (%s, %zu bytes)\n", file->path, file->mime, file->size);
}

int main(void)
{
    printf("CIRF Simple Example\n");
    printf("===================\n\n");

    /* ========================================
     * Direct access using exposed symbols
     * (Does NOT require cirf_runtime library)
     * ======================================== */
    printf("Direct symbol access (no runtime needed):\n");
    printf("------------------------------------------\n");

    /* Access root metadata directly */
    printf("Root has %zu files, %zu subdirectories\n",
           simple_resources_root.file_count,
           simple_resources_root.child_count);

    /* Access file directly by symbol (no lookup needed) */
    printf("Direct: simple_resources_file_hello_txt->size = %zu\n",
           simple_resources_file_hello_txt->size);
    printf("Direct: simple_resources_file_hello_txt->mime = %s\n",
           simple_resources_file_hello_txt->mime);

    /* Access folder directly */
    printf("Direct: simple_resources_dir_config.file_count = %zu\n",
           simple_resources_dir_config.file_count);

    /* Access nested file directly */
    printf("Direct: simple_resources_file_config_data_json->path = %s\n\n",
           simple_resources_file_config_data_json->path);

    /* ========================================
     * Using cirf_runtime library functions
     * (Requires linking against cirf_runtime)
     * ======================================== */
    printf("Runtime library functions:\n");
    printf("--------------------------\n");

    /* Get metadata using runtime helper */
    const char *version = cirf_get_metadata(
        simple_resources_root.metadata,
        simple_resources_root.metadata_count,
        "version"
    );
    printf("Resource version: %s\n\n", version ? version : "unknown");

    /* List all files using runtime iterator */
    printf("All embedded files:\n");
    cirf_foreach_file_recursive(&simple_resources_root, print_file, NULL);
    printf("\n");

    /* Find file by path using runtime lookup */
    const cirf_file_t *hello = cirf_find_file(&simple_resources_root, "hello.txt");
    if (hello) {
        printf("Contents of %s:\n", hello->path);
        printf("---\n");
        fwrite(hello->data, 1, hello->size, stdout);
        printf("---\n\n");

        /* Show file metadata using runtime helper */
        const char *desc = cirf_get_metadata(
            hello->metadata, hello->metadata_count, "description"
        );
        if (desc) {
            printf("Description: %s\n\n", desc);
        }

        /* Navigate to parent */
        printf("Parent folder: %s\n",
               hello->parent ? (hello->parent->path[0] ? hello->parent->path : "(root)") : "none");
    }

    /* ========================================
     * FILE* integration using cirf_fopen()
     * (Uses fmemopen on POSIX systems)
     * ======================================== */
    printf("\nFILE* integration demo:\n");
    printf("-----------------------\n");

    FILE *fp = cirf_fopen_path(&simple_resources_root, "config/data.json");
    if (fp) {
        printf("Reading config/data.json via FILE*:\n");
        printf("---\n");
        char buf[256];
        while (fgets(buf, sizeof(buf), fp)) {
            printf("%s", buf);
        }
        printf("---\n");
        fclose(fp);
    } else {
        printf("cirf_fopen_path() not available (non-POSIX system?)\n");
    }

    return 0;
}
