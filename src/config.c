#include "cirf/config.h"
#include "cirf/glob.h"
#include "cirf/json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *strdup_local(const char *s) {
    if(!s) return NULL;
    size_t len = strlen(s);
    char  *dup = malloc(len + 1);
    if(dup) {
        memcpy(dup, s, len + 1);
    }
    return dup;
}

static char *path_dirname(const char *path) {
    if(!path) return strdup_local("");

    const char *last_slash = strrchr(path, '/');
    if(!last_slash) {
        /* No slash means file in current directory - return empty string */
        return strdup_local("");
    }

    size_t len = (size_t)(last_slash - path);
    if(len == 0) {
        return strdup_local("/");
    }

    char *result = malloc(len + 1);
    if(!result) return NULL;

    memcpy(result, path, len);
    result[len] = '\0';
    return result;
}

static char *path_basename(const char *path) {
    if(!path) return NULL;

    const char *last_slash = strrchr(path, '/');
    if(last_slash) {
        return strdup_local(last_slash + 1);
    }
    return strdup_local(path);
}

static char *path_join(const char *a, const char *b) {
    if(!a || !*a) return strdup_local(b);
    if(!b || !*b) return strdup_local(a);

    /* Skip leading ./ from second path */
    while(b[0] == '.' && b[1] == '/') {
        b += 2;
    }

    size_t a_len = strlen(a);
    size_t b_len = strlen(b);
    int    need_sep = (a[a_len - 1] != '/');

    char *result = malloc(a_len + need_sep + b_len + 1);
    if(!result) return NULL;

    memcpy(result, a, a_len);
    if(need_sep) {
        result[a_len] = '/';
    }
    memcpy(result + a_len + need_sep, b, b_len + 1);

    return result;
}

static cirf_error_t load_metadata(const json_value_t *obj, vfs_metadata_t **out) {
    json_value_t *meta = json_get(obj, "metadata");
    if(!meta || meta->type != JSON_OBJECT) {
        return CIRF_OK;
    }

    for(size_t i = 0; i < meta->data.object.count; i++) {
        const char   *key = meta->data.object.keys[i];
        json_value_t *val = &meta->data.object.values[i];

        if(val->type == JSON_STRING) {
            vfs_add_metadata(out, key, val->data.string);
        }
    }

    return CIRF_OK;
}

typedef struct {
        cirf_config_t      *config;
        const char         *target;
        const json_value_t *glob_meta;
} glob_ctx_t;

static int glob_callback(const char *path, void *ctx) {
    glob_ctx_t *gctx = ctx;
    char       *basename = path_basename(path);
    if(!basename) return -1;

    char *target_path = path_join(gctx->target, basename);
    free(basename);
    if(!target_path) return -1;

    /* Ensure parent folder exists */
    char         *folder_path = path_dirname(target_path);
    vfs_folder_t *folder = vfs_ensure_folder(gctx->config->root, folder_path);
    free(folder_path);

    if(!folder) {
        free(target_path);
        return -1;
    }

    /* Get just the filename */
    char *filename = path_basename(target_path);
    free(target_path);
    if(!filename) return -1;

    vfs_file_t *file = vfs_add_file(folder, filename, path);
    free(filename);

    if(!file) {
        return 0; /* May be duplicate, continue */
    }

    /* Apply metadata from glob entry */
    if(gctx->glob_meta) {
        load_metadata(gctx->glob_meta, &file->metadata);
    }

    return 0;
}

static cirf_error_t process_entry(cirf_config_t *config, const json_value_t *entry,
                                  vfs_folder_t *parent_folder);

static cirf_error_t process_file_entry(cirf_config_t *config, const json_value_t *entry,
                                       vfs_folder_t *parent_folder) {
    const char *path = json_get_string(entry, "path");
    const char *source = json_get_string(entry, "source");

    if(!path || !source) {
        return CIRF_ERR_INVALID;
    }

    /* Resolve source path relative to config directory */
    char *full_source = path_join(config->base_dir, source);
    if(!full_source) {
        return CIRF_ERR_NOMEM;
    }

    /* Find parent folder path and filename */
    char *folder_path = path_dirname(path);
    char *filename = path_basename(path);

    if(!folder_path || !filename) {
        free(full_source);
        free(folder_path);
        free(filename);
        return CIRF_ERR_NOMEM;
    }

    /* Combine parent folder path with entry's folder path */
    char *full_folder_path;
    if(folder_path[0] == '\0') {
        /* File is directly in parent folder */
        full_folder_path = strdup_local(parent_folder->path);
    } else if(parent_folder->path[0] == '\0') {
        /* Parent is root, use folder_path directly */
        full_folder_path = strdup_local(folder_path);
    } else {
        full_folder_path = path_join(parent_folder->path, folder_path);
    }
    free(folder_path);

    if(!full_folder_path) {
        free(full_source);
        free(filename);
        return CIRF_ERR_NOMEM;
    }

    vfs_folder_t *folder = vfs_ensure_folder(config->root, full_folder_path);
    free(full_folder_path);

    if(!folder) {
        free(full_source);
        free(filename);
        return CIRF_ERR_NOMEM;
    }

    vfs_file_t *file = vfs_add_file(folder, filename, full_source);
    free(full_source);
    free(filename);

    if(!file) {
        return CIRF_ERR_DUPLICATE;
    }

    /* Override MIME type if specified */
    const char *mime = json_get_string(entry, "mime");
    if(mime) {
        free(file->mime);
        file->mime = strdup_local(mime);
    }

    /* Load metadata */
    load_metadata(entry, &file->metadata);

    return CIRF_OK;
}

static cirf_error_t process_folder_entry(cirf_config_t *config, const json_value_t *entry,
                                         vfs_folder_t *parent_folder) {
    const char *path = json_get_string(entry, "path");
    if(!path) {
        return CIRF_ERR_INVALID;
    }

    /* Build full folder path */
    char *full_path;
    if(parent_folder->path[0] == '\0') {
        full_path = strdup_local(path);
    } else {
        full_path = path_join(parent_folder->path, path);
    }

    if(!full_path) {
        return CIRF_ERR_NOMEM;
    }

    vfs_folder_t *folder = vfs_ensure_folder(config->root, full_path);
    free(full_path);

    if(!folder) {
        return CIRF_ERR_NOMEM;
    }

    /* Load metadata */
    load_metadata(entry, &folder->metadata);

    /* Process nested entries */
    json_value_t *entries = json_get(entry, "entries");
    if(entries && entries->type == JSON_ARRAY) {
        for(size_t i = 0; i < entries->data.array.count; i++) {
            cirf_error_t err = process_entry(config, &entries->data.array.items[i], folder);
            if(err != CIRF_OK) {
                return err;
            }
        }
    }

    return CIRF_OK;
}

static cirf_error_t process_glob_entry(cirf_config_t *config, const json_value_t *entry,
                                       vfs_folder_t *parent_folder) {
    const char *pattern = json_get_string(entry, "pattern");
    const char *target = json_get_string(entry, "target");

    if(!pattern || !target) {
        return CIRF_ERR_INVALID;
    }

    /* Build full target path */
    char *full_target;
    if(parent_folder->path[0] == '\0') {
        full_target = strdup_local(target);
    } else {
        full_target = path_join(parent_folder->path, target);
    }

    if(!full_target) {
        return CIRF_ERR_NOMEM;
    }

    glob_ctx_t ctx = {.config = config, .target = full_target, .glob_meta = entry};

    cirf_error_t err = glob_match(pattern, config->base_dir, glob_callback, &ctx);
    free(full_target);

    return err;
}

static cirf_error_t process_entry(cirf_config_t *config, const json_value_t *entry,
                                  vfs_folder_t *parent_folder) {
    if(!entry || entry->type != JSON_OBJECT) {
        return CIRF_ERR_INVALID;
    }

    const char *type = json_get_string(entry, "type");
    if(!type) {
        return CIRF_ERR_INVALID;
    }

    if(strcmp(type, "file") == 0) {
        return process_file_entry(config, entry, parent_folder);
    } else if(strcmp(type, "folder") == 0) {
        return process_folder_entry(config, entry, parent_folder);
    } else if(strcmp(type, "glob") == 0) {
        return process_glob_entry(config, entry, parent_folder);
    }

    return CIRF_ERR_INVALID;
}

cirf_error_t config_load(const char *path, const char *name, cirf_config_t **out) {
    if(!path || !name || !out) {
        return CIRF_ERR_INVALID;
    }

    json_value_t *json = NULL;
    cirf_error_t  err = json_parse_file(path, &json);
    if(err != CIRF_OK) {
        return err;
    }

    if(json->type != JSON_OBJECT) {
        json_destroy(json);
        return CIRF_ERR_PARSE;
    }

    cirf_config_t *config = calloc(1, sizeof(cirf_config_t));
    if(!config) {
        json_destroy(json);
        return CIRF_ERR_NOMEM;
    }

    config->name = strdup_local(name);
    config->base_dir = path_dirname(path);
    config->root = vfs_create_root();

    if(!config->name || !config->base_dir || !config->root) {
        config_destroy(config);
        json_destroy(json);
        return CIRF_ERR_NOMEM;
    }

    /* Load root metadata */
    load_metadata(json, &config->root->metadata);

    /* Process entries */
    json_value_t *entries = json_get(json, "entries");
    if(entries && entries->type == JSON_ARRAY) {
        for(size_t i = 0; i < entries->data.array.count; i++) {
            err = process_entry(config, &entries->data.array.items[i], config->root);
            if(err != CIRF_OK) {
                config_destroy(config);
                json_destroy(json);
                return err;
            }
        }
    }

    json_destroy(json);

    /* Load all file data */
    err = vfs_load_all_data(config->root);
    if(err != CIRF_OK) {
        config_destroy(config);
        return err;
    }

    *out = config;
    return CIRF_OK;
}

void config_destroy(cirf_config_t *config) {
    if(!config) return;
    free(config->name);
    free(config->base_dir);
    vfs_destroy(config->root);
    free(config);
}

cirf_error_t config_load_deps(const char *path, const char *name, cirf_config_t **out) {
    if(!path || !name || !out) {
        return CIRF_ERR_INVALID;
    }

    json_value_t *json = NULL;
    cirf_error_t  err = json_parse_file(path, &json);
    if(err != CIRF_OK) {
        return err;
    }

    if(json->type != JSON_OBJECT) {
        json_destroy(json);
        return CIRF_ERR_PARSE;
    }

    cirf_config_t *config = calloc(1, sizeof(cirf_config_t));
    if(!config) {
        json_destroy(json);
        return CIRF_ERR_NOMEM;
    }

    config->name = strdup_local(name);
    config->base_dir = path_dirname(path);
    config->root = vfs_create_root();

    if(!config->name || !config->base_dir || !config->root) {
        config_destroy(config);
        json_destroy(json);
        return CIRF_ERR_NOMEM;
    }

    /* Load root metadata */
    load_metadata(json, &config->root->metadata);

    /* Process entries */
    json_value_t *entries = json_get(json, "entries");
    if(entries && entries->type == JSON_ARRAY) {
        for(size_t i = 0; i < entries->data.array.count; i++) {
            err = process_entry(config, &entries->data.array.items[i], config->root);
            if(err != CIRF_OK) {
                config_destroy(config);
                json_destroy(json);
                return err;
            }
        }
    }

    json_destroy(json);

    /* Skip loading file data - just return the structure with source paths */
    *out = config;
    return CIRF_OK;
}

static void collect_source_paths_folder(const vfs_folder_t *folder, char **buf, size_t *len,
                                        size_t *cap) {
    /* Collect files in this folder */
    for(vfs_file_t *file = folder->files; file; file = file->next) {
        if(file->source_path) {
            size_t path_len = strlen(file->source_path);
            size_t needed = *len + path_len + 2; /* +1 for newline, +1 for null */

            if(needed > *cap) {
                size_t new_cap = *cap * 2;
                if(new_cap < needed) new_cap = needed;
                char *new_buf = realloc(*buf, new_cap);
                if(!new_buf) return;
                *buf = new_buf;
                *cap = new_cap;
            }

            memcpy(*buf + *len, file->source_path, path_len);
            *len += path_len;
            (*buf)[(*len)++] = '\n';
        }
    }

    /* Recurse into child folders */
    for(vfs_folder_t *child = folder->children; child; child = child->next) {
        collect_source_paths_folder(child, buf, len, cap);
    }
}

char *config_get_source_paths(const cirf_config_t *config) {
    if(!config || !config->root) return NULL;

    size_t cap = 1024;
    size_t len = 0;
    char  *buf = malloc(cap);
    if(!buf) return NULL;

    collect_source_paths_folder(config->root, &buf, &len, &cap);

    /* Null-terminate */
    if(len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0'; /* Replace trailing newline with null */
    } else {
        if(len + 1 > cap) {
            char *new_buf = realloc(buf, len + 1);
            if(!new_buf) {
                free(buf);
                return NULL;
            }
            buf = new_buf;
        }
        buf[len] = '\0';
    }

    return buf;
}
