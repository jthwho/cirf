#include "cirf/glob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static char *strdup_local(const char *s)
{
    if (!s) return NULL;
    size_t len = strlen(s);
    char *dup = malloc(len + 1);
    if (dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

static char *path_join(const char *a, const char *b)
{
    if (!a || !*a) return strdup_local(b);
    if (!b || !*b) return strdup_local(a);

    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    int need_sep = (a[a_len - 1] != '/');

    char *result = malloc(a_len + need_sep + b_len + 1);
    if (!result) return NULL;

    memcpy(result, a, a_len);
    if (need_sep) {
        result[a_len] = '/';
    }
    memcpy(result + a_len + need_sep, b, b_len + 1);

    return result;
}

static int is_directory(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode);
}

int glob_pattern_match(const char *pattern, const char *string)
{
    while (*pattern && *string) {
        if (*pattern == '*') {
            pattern++;

            if (*pattern == '*') {
                /* ** matches anything including / */
                pattern++;
                if (*pattern == '/') {
                    pattern++;
                }
                /* Try matching rest of pattern at each position */
                while (*string) {
                    if (glob_pattern_match(pattern, string)) {
                        return 1;
                    }
                    string++;
                }
                return glob_pattern_match(pattern, string);
            }

            /* * matches anything except / */
            while (*string && *string != '/') {
                if (glob_pattern_match(pattern, string)) {
                    return 1;
                }
                string++;
            }
            continue;
        }

        if (*pattern == '?') {
            if (*string == '/') {
                return 0;
            }
            pattern++;
            string++;
            continue;
        }

        if (*pattern != *string) {
            return 0;
        }

        pattern++;
        string++;
    }

    /* Skip trailing ** */
    while (*pattern == '*') {
        pattern++;
    }

    return *pattern == '\0' && *string == '\0';
}

static cirf_error_t glob_recurse(const char *dir_path, const char *pattern,
                                  const char *prefix, glob_callback_t callback,
                                  void *ctx)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return CIRF_ERR_IO;
    }

    struct dirent *entry;
    cirf_error_t err = CIRF_OK;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char *full_path = path_join(dir_path, entry->d_name);
        if (!full_path) {
            err = CIRF_ERR_NOMEM;
            break;
        }

        char *rel_path = path_join(prefix, entry->d_name);
        if (!rel_path) {
            free(full_path);
            err = CIRF_ERR_NOMEM;
            break;
        }

        if (is_directory(full_path)) {
            /* Recurse into subdirectories */
            err = glob_recurse(full_path, pattern, rel_path, callback, ctx);
            if (err != CIRF_OK) {
                free(full_path);
                free(rel_path);
                break;
            }
        } else {
            /* Check if file matches pattern */
            if (glob_pattern_match(pattern, rel_path)) {
                int result = callback(full_path, ctx);
                if (result != 0) {
                    free(full_path);
                    free(rel_path);
                    break;
                }
            }
        }

        free(full_path);
        free(rel_path);
    }

    closedir(dir);
    return err;
}

cirf_error_t glob_match(const char *pattern, const char *base_dir,
                         glob_callback_t callback, void *ctx)
{
    if (!pattern || !callback) {
        return CIRF_ERR_INVALID;
    }

    const char *dir = base_dir ? base_dir : ".";

    /* Handle patterns starting with ./ */
    const char *pat = pattern;
    if (pat[0] == '.' && pat[1] == '/') {
        pat += 2;
    }

    return glob_recurse(dir, pat, "", callback, ctx);
}
