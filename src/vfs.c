#include "cirf/vfs.h"
#include "cirf/mime.h"
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

static void metadata_destroy(vfs_metadata_t *metadata) {
    while(metadata) {
        vfs_metadata_t *next = metadata->next;
        free(metadata->key);
        free(metadata->value);
        free(metadata);
        metadata = next;
    }
}

static void file_destroy(vfs_file_t *file) {
    while(file) {
        vfs_file_t *next = file->next;
        free(file->name);
        free(file->path);
        free(file->source_path);
        free(file->mime);
        free(file->data);
        metadata_destroy(file->metadata);
        free(file);
        file = next;
    }
}

static void folder_destroy(vfs_folder_t *folder) {
    if(!folder) return;

    vfs_folder_t *child = folder->children;
    while(child) {
        vfs_folder_t *next = child->next;
        folder_destroy(child);
        child = next;
    }

    file_destroy(folder->files);
    metadata_destroy(folder->metadata);
    free(folder->name);
    free(folder->path);
    free(folder);
}

vfs_folder_t *vfs_create_root(void) {
    vfs_folder_t *root = calloc(1, sizeof(vfs_folder_t));
    if(!root) return NULL;

    root->name = strdup_local("");
    root->path = strdup_local("");
    if(!root->name || !root->path) {
        free(root->name);
        free(root->path);
        free(root);
        return NULL;
    }

    return root;
}

void vfs_destroy(vfs_folder_t *root) {
    folder_destroy(root);
}

static vfs_folder_t *folder_find_child(vfs_folder_t *parent, const char *name) {
    for(vfs_folder_t *child = parent->children; child; child = child->next) {
        if(strcmp(child->name, name) == 0) {
            return child;
        }
    }
    return NULL;
}

vfs_folder_t *vfs_add_folder(vfs_folder_t *parent, const char *name) {
    if(!parent || !name) return NULL;

    /* Check if already exists */
    vfs_folder_t *existing = folder_find_child(parent, name);
    if(existing) {
        return existing;
    }

    vfs_folder_t *folder = calloc(1, sizeof(vfs_folder_t));
    if(!folder) return NULL;

    folder->name = strdup_local(name);
    if(!folder->name) {
        free(folder);
        return NULL;
    }

    /* Build full path */
    size_t parent_len = strlen(parent->path);
    size_t name_len = strlen(name);
    size_t path_len = parent_len + (parent_len > 0 ? 1 : 0) + name_len;

    folder->path = malloc(path_len + 1);
    if(!folder->path) {
        free(folder->name);
        free(folder);
        return NULL;
    }

    if(parent_len > 0) {
        snprintf(folder->path, path_len + 1, "%s/%s", parent->path, name);
    } else {
        strcpy(folder->path, name);
    }

    folder->parent = parent;

    /* Add to parent's children (at end to preserve order) */
    if(!parent->children) {
        parent->children = folder;
    } else {
        vfs_folder_t *last = parent->children;
        while(last->next) {
            last = last->next;
        }
        last->next = folder;
    }

    return folder;
}

vfs_folder_t *vfs_find_folder(vfs_folder_t *root, const char *path) {
    if(!root || !path) return NULL;
    if(*path == '\0') return root;

    char *path_copy = strdup_local(path);
    if(!path_copy) return NULL;

    vfs_folder_t *current = root;
    char         *saveptr = NULL;
    char         *token = strtok_r(path_copy, "/", &saveptr);

    while(token && current) {
        current = folder_find_child(current, token);
        token = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);
    return current;
}

vfs_folder_t *vfs_ensure_folder(vfs_folder_t *root, const char *path) {
    if(!root || !path) return NULL;
    if(*path == '\0') return root;

    char *path_copy = strdup_local(path);
    if(!path_copy) return NULL;

    vfs_folder_t *current = root;
    char         *saveptr = NULL;
    char         *token = strtok_r(path_copy, "/", &saveptr);

    while(token && current) {
        vfs_folder_t *child = folder_find_child(current, token);
        if(!child) {
            child = vfs_add_folder(current, token);
        }
        current = child;
        token = strtok_r(NULL, "/", &saveptr);
    }

    free(path_copy);
    return current;
}

static vfs_file_t *folder_find_file(vfs_folder_t *folder, const char *name) {
    for(vfs_file_t *file = folder->files; file; file = file->next) {
        if(strcmp(file->name, name) == 0) {
            return file;
        }
    }
    return NULL;
}

vfs_file_t *vfs_add_file(vfs_folder_t *parent, const char *name, const char *source_path) {
    if(!parent || !name) return NULL;

    /* Check if already exists */
    if(folder_find_file(parent, name)) {
        return NULL; /* Duplicate */
    }

    vfs_file_t *file = calloc(1, sizeof(vfs_file_t));
    if(!file) return NULL;

    file->name = strdup_local(name);
    file->source_path = source_path ? strdup_local(source_path) : NULL;

    if(!file->name || (source_path && !file->source_path)) {
        free(file->name);
        free(file->source_path);
        free(file);
        return NULL;
    }

    /* Build full path */
    size_t parent_len = strlen(parent->path);
    size_t name_len = strlen(name);
    size_t path_len = parent_len + (parent_len > 0 ? 1 : 0) + name_len;

    file->path = malloc(path_len + 1);
    if(!file->path) {
        free(file->name);
        free(file->source_path);
        free(file);
        return NULL;
    }

    if(parent_len > 0) {
        snprintf(file->path, path_len + 1, "%s/%s", parent->path, name);
    } else {
        strcpy(file->path, name);
    }

    /* Auto-detect MIME type */
    file->mime = strdup_local(mime_from_path(name));

    file->parent = parent;

    /* Add to parent's files (at end to preserve order) */
    if(!parent->files) {
        parent->files = file;
    } else {
        vfs_file_t *last = parent->files;
        while(last->next) {
            last = last->next;
        }
        last->next = file;
    }

    return file;
}

vfs_file_t *vfs_find_file(vfs_folder_t *root, const char *path) {
    if(!root || !path) return NULL;

    /* Find last slash to split into folder path and filename */
    const char *last_slash = strrchr(path, '/');
    if(!last_slash) {
        /* File in root folder */
        return folder_find_file(root, path);
    }

    /* Extract folder path */
    size_t folder_len = (size_t)(last_slash - path);
    char  *folder_path = malloc(folder_len + 1);
    if(!folder_path) return NULL;

    memcpy(folder_path, path, folder_len);
    folder_path[folder_len] = '\0';

    vfs_folder_t *folder = vfs_find_folder(root, folder_path);
    free(folder_path);

    if(!folder) return NULL;

    return folder_find_file(folder, last_slash + 1);
}

cirf_error_t vfs_load_file_data(vfs_file_t *file) {
    if(!file || !file->source_path) {
        return CIRF_ERR_INVALID;
    }

    if(file->data) {
        return CIRF_OK; /* Already loaded */
    }

    FILE *fp = fopen(file->source_path, "rb");
    if(!fp) {
        return CIRF_ERR_IO;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if(size < 0) {
        fclose(fp);
        return CIRF_ERR_IO;
    }

    unsigned char *data = malloc((size_t)size);
    if(!data) {
        fclose(fp);
        return CIRF_ERR_NOMEM;
    }

    size_t read = fread(data, 1, (size_t)size, fp);
    fclose(fp);

    if((long)read != size) {
        free(data);
        return CIRF_ERR_IO;
    }

    file->data = data;
    file->size = (size_t)size;
    return CIRF_OK;
}

static cirf_error_t load_folder_data(vfs_folder_t *folder) {
    cirf_error_t err;

    for(vfs_file_t *file = folder->files; file; file = file->next) {
        err = vfs_load_file_data(file);
        if(err != CIRF_OK) {
            return err;
        }
    }

    for(vfs_folder_t *child = folder->children; child; child = child->next) {
        err = load_folder_data(child);
        if(err != CIRF_OK) {
            return err;
        }
    }

    return CIRF_OK;
}

cirf_error_t vfs_load_all_data(vfs_folder_t *root) {
    if(!root) return CIRF_ERR_INVALID;
    return load_folder_data(root);
}

void vfs_add_metadata(vfs_metadata_t **list, const char *key, const char *value) {
    if(!list || !key || !value) return;

    vfs_metadata_t *meta = calloc(1, sizeof(vfs_metadata_t));
    if(!meta) return;

    meta->key = strdup_local(key);
    meta->value = strdup_local(value);

    if(!meta->key || !meta->value) {
        free(meta->key);
        free(meta->value);
        free(meta);
        return;
    }

    /* Add at end to preserve order */
    if(!*list) {
        *list = meta;
    } else {
        vfs_metadata_t *last = *list;
        while(last->next) {
            last = last->next;
        }
        last->next = meta;
    }
}

const char *vfs_get_metadata(const vfs_metadata_t *list, const char *key) {
    for(const vfs_metadata_t *m = list; m; m = m->next) {
        if(strcmp(m->key, key) == 0) {
            return m->value;
        }
    }
    return NULL;
}

size_t vfs_metadata_count(const vfs_metadata_t *list) {
    size_t count = 0;
    for(const vfs_metadata_t *m = list; m; m = m->next) {
        count++;
    }
    return count;
}

size_t vfs_folder_count(const vfs_folder_t *folder) {
    size_t count = 0;
    for(const vfs_folder_t *c = folder->children; c; c = c->next) {
        count++;
    }
    return count;
}

size_t vfs_file_count(const vfs_folder_t *folder) {
    size_t count = 0;
    for(const vfs_file_t *f = folder->files; f; f = f->next) {
        count++;
    }
    return count;
}
