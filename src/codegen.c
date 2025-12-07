#include "cirf/codegen.h"
#include "cirf/writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const char *name;
    writer_t *w;
    int file_index;
    int folder_index;
    int metadata_index;
} codegen_ctx_t;

static char *make_identifier(const char *path)
{
    if (!path || !*path) {
        return strdup("root");
    }

    size_t len = strlen(path);
    char *id = malloc(len + 1);
    if (!id) return NULL;

    for (size_t i = 0; i < len; i++) {
        char c = path[i];
        if (isalnum((unsigned char)c)) {
            id[i] = c;
        } else {
            id[i] = '_';
        }
    }
    id[len] = '\0';
    return id;
}

static char *make_file_symbol(const char *prefix, const char *path)
{
    char *id = make_identifier(path);
    if (!id) return NULL;

    size_t prefix_len = strlen(prefix);
    size_t id_len = strlen(id);
    /* prefix + "_file_" + id + null */
    char *result = malloc(prefix_len + 6 + id_len + 1);
    if (!result) {
        free(id);
        return NULL;
    }
    sprintf(result, "%s_file_%s", prefix, id);
    free(id);
    return result;
}

static char *make_dir_symbol(const char *prefix, const char *path)
{
    if (!path || !*path) {
        /* Root folder */
        size_t prefix_len = strlen(prefix);
        char *result = malloc(prefix_len + 5 + 1);
        if (!result) return NULL;
        sprintf(result, "%s_root", prefix);
        return result;
    }

    char *id = make_identifier(path);
    if (!id) return NULL;

    size_t prefix_len = strlen(prefix);
    size_t id_len = strlen(id);
    /* prefix + "_dir_" + id + null */
    char *result = malloc(prefix_len + 5 + id_len + 1);
    if (!result) {
        free(id);
        return NULL;
    }
    sprintf(result, "%s_dir_%s", prefix, id);
    free(id);
    return result;
}


static void generate_file_extern_decls(writer_t *w, const char *name, const vfs_folder_t *folder)
{
    for (const vfs_file_t *f = folder->files; f; f = f->next) {
        char *sym = make_file_symbol(name, f->path);
        if (sym) {
            writer_printf(w, "extern const cirf_file_t * const %s;\n", sym);
            free(sym);
        }
    }

    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        generate_file_extern_decls(w, name, c);
    }
}

static void generate_folder_extern_decls(writer_t *w, const char *name, const vfs_folder_t *folder)
{
    /* Generate for children first */
    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        char *sym = make_dir_symbol(name, c->path);
        if (sym) {
            writer_printf(w, "extern const cirf_folder_t %s;\n", sym);
            free(sym);
        }
        generate_folder_extern_decls(w, name, c);
    }
}

static void generate_file_data(codegen_ctx_t *ctx, const vfs_file_t *file, int index)
{
    writer_printf(ctx->w, "static const unsigned char %s_data_%d[] = {\n", ctx->name, index);
    writer_indent(ctx->w);

    if (file->size > 0) {
        writer_write_bytes_hex(ctx->w, file->data, file->size, 12);
    }

    writer_newline(ctx->w);
    writer_dedent(ctx->w);
    writer_printf(ctx->w, "};\n\n");
}

static int generate_metadata(codegen_ctx_t *ctx, const vfs_metadata_t *meta)
{
    if (!meta) return -1;

    int index = ctx->metadata_index++;
    size_t count = vfs_metadata_count(meta);

    writer_printf(ctx->w, "static const cirf_metadata_t %s_meta_%d[] = {\n",
                  ctx->name, index);
    writer_indent(ctx->w);

    for (const vfs_metadata_t *m = meta; m; m = m->next) {
        writer_puts(ctx->w, "{ ");
        writer_write_string_escaped(ctx->w, m->key);
        writer_puts(ctx->w, ", ");
        writer_write_string_escaped(ctx->w, m->value);
        writer_puts(ctx->w, " }");
        if (m->next) {
            writer_puts(ctx->w, ",");
        }
        writer_newline(ctx->w);
    }

    writer_dedent(ctx->w);
    writer_printf(ctx->w, "};\n\n");

    (void)count;
    return index;
}

static void generate_folder_forward_decl(codegen_ctx_t *ctx, const vfs_folder_t *folder)
{
    char *sym = make_dir_symbol(ctx->name, folder->path);
    if (sym) {
        writer_printf(ctx->w, "const cirf_folder_t %s;\n", sym);
        free(sym);
    }
}

static void generate_all_data(codegen_ctx_t *ctx, const vfs_folder_t *folder)
{
    for (const vfs_file_t *f = folder->files; f; f = f->next) {
        generate_file_data(ctx, f, ctx->file_index++);
    }

    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        generate_all_data(ctx, c);
    }
}

typedef struct file_meta_info {
    const vfs_file_t *file;
    int metadata_index;
    struct file_meta_info *next;
} file_meta_info_t;

typedef struct folder_info {
    int self_index;
    int files_start;
    int files_count;
    int children_start;
    int children_count;
    int metadata_index;
    const vfs_folder_t *folder;
    struct folder_info *next;
} folder_info_t;

static void collect_folder_info(const vfs_folder_t *folder,
                                 folder_info_t **list,
                                 int *file_idx, int *folder_idx)
{
    folder_info_t *info = calloc(1, sizeof(folder_info_t));
    info->folder = folder;
    info->self_index = (*folder_idx)++;
    info->files_start = *file_idx;
    info->files_count = (int)vfs_file_count(folder);
    (*file_idx) += info->files_count;
    info->children_start = *folder_idx;
    info->children_count = (int)vfs_folder_count(folder);
    info->metadata_index = -1;

    /* Add to list */
    if (!*list) {
        *list = info;
    } else {
        folder_info_t *last = *list;
        while (last->next) last = last->next;
        last->next = info;
    }

    /* Recurse for children - they get consecutive indices */
    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        collect_folder_info(c, list, file_idx, folder_idx);
    }
}

static folder_info_t *find_folder_info(folder_info_t *list, const vfs_folder_t *folder)
{
    for (folder_info_t *i = list; i; i = i->next) {
        if (i->folder == folder) return i;
    }
    return NULL;
}

static void free_folder_info(folder_info_t *list)
{
    while (list) {
        folder_info_t *next = list->next;
        free(list);
        list = next;
    }
}

static void free_file_meta_info(file_meta_info_t *list)
{
    while (list) {
        file_meta_info_t *next = list->next;
        free(list);
        list = next;
    }
}

static int find_file_meta_index(file_meta_info_t *list, const vfs_file_t *file)
{
    for (file_meta_info_t *m = list; m; m = m->next) {
        if (m->file == file) {
            return m->metadata_index;
        }
    }
    return -1;
}

static void generate_all_file_metadata(codegen_ctx_t *ctx, const vfs_folder_t *folder,
                                         file_meta_info_t **list)
{
    for (const vfs_file_t *f = folder->files; f; f = f->next) {
        if (f->metadata) {
            file_meta_info_t *info = calloc(1, sizeof(file_meta_info_t));
            if (info) {
                info->file = f;
                info->metadata_index = generate_metadata(ctx, f->metadata);
                info->next = *list;
                *list = info;
            }
        }
    }

    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        generate_all_file_metadata(ctx, c, list);
    }
}

static void generate_files_array(codegen_ctx_t *ctx, const vfs_folder_t *folder,
                                  folder_info_t *info_list, file_meta_info_t *file_meta_list,
                                  int *file_idx)
{
    if (!folder->files) return;

    folder_info_t *folder_info = find_folder_info(info_list, folder);
    if (!folder_info) return;

    /* Use path-based name for files array */
    char *dir_sym = make_dir_symbol(ctx->name, folder->path);
    if (!dir_sym) return;

    writer_printf(ctx->w, "static const cirf_file_t %s_files[] = {\n", dir_sym);
    free(dir_sym);
    writer_indent(ctx->w);

    for (const vfs_file_t *f = folder->files; f; f = f->next) {
        int meta_idx = find_file_meta_index(file_meta_list, f);

        writer_puts(ctx->w, "{\n");
        writer_indent(ctx->w);

        writer_puts(ctx->w, ".name = ");
        writer_write_string_escaped(ctx->w, f->name);
        writer_puts(ctx->w, ",\n");

        writer_puts(ctx->w, ".path = ");
        writer_write_string_escaped(ctx->w, f->path);
        writer_puts(ctx->w, ",\n");

        writer_puts(ctx->w, ".mime = ");
        writer_write_string_escaped(ctx->w, f->mime ? f->mime : "application/octet-stream");
        writer_puts(ctx->w, ",\n");

        writer_printf(ctx->w, ".data = %s_data_%d,\n", ctx->name, *file_idx);
        writer_printf(ctx->w, ".size = %zu,\n", f->size);

        /* Parent pointer using path-based name */
        char *parent_sym = make_dir_symbol(ctx->name, folder->path);
        if (parent_sym) {
            writer_printf(ctx->w, ".parent = &%s,\n", parent_sym);
            free(parent_sym);
        }

        if (meta_idx >= 0) {
            writer_printf(ctx->w, ".metadata = %s_meta_%d,\n", ctx->name, meta_idx);
            writer_printf(ctx->w, ".metadata_count = %zu\n", vfs_metadata_count(f->metadata));
        } else {
            writer_puts(ctx->w, ".metadata = NULL,\n");
            writer_puts(ctx->w, ".metadata_count = 0\n");
        }

        writer_dedent(ctx->w);
        writer_puts(ctx->w, "}");
        if (f->next) {
            writer_puts(ctx->w, ",");
        }
        writer_newline(ctx->w);

        (*file_idx)++;
    }

    writer_dedent(ctx->w);
    writer_printf(ctx->w, "};\n\n");

    /* Generate individual file pointer aliases */
    char *arr_sym = make_dir_symbol(ctx->name, folder->path);
    if (arr_sym) {
        int file_index = 0;
        for (const vfs_file_t *f = folder->files; f; f = f->next) {
            char *file_sym = make_file_symbol(ctx->name, f->path);
            if (file_sym) {
                writer_printf(ctx->w, "const cirf_file_t * const %s = &%s_files[%d];\n",
                              file_sym, arr_sym, file_index);
                free(file_sym);
            }
            file_index++;
        }
        free(arr_sym);
        writer_newline(ctx->w);
    }
}

static void generate_folder_struct(codegen_ctx_t *ctx, const vfs_folder_t *folder,
                                    folder_info_t *info_list)
{
    folder_info_t *info = find_folder_info(info_list, folder);
    if (!info) return;

    /* Generate metadata if present */
    if (folder->metadata) {
        info->metadata_index = generate_metadata(ctx, folder->metadata);
    }

    /* Use path-based symbol name */
    char *self_sym = make_dir_symbol(ctx->name, folder->path);
    if (!self_sym) return;

    writer_printf(ctx->w, "const cirf_folder_t %s = {\n", self_sym);
    free(self_sym);
    writer_indent(ctx->w);

    writer_puts(ctx->w, ".name = ");
    writer_write_string_escaped(ctx->w, folder->name);
    writer_puts(ctx->w, ",\n");

    writer_puts(ctx->w, ".path = ");
    writer_write_string_escaped(ctx->w, folder->path);
    writer_puts(ctx->w, ",\n");

    /* Parent pointer using path-based name */
    if (folder->parent) {
        char *parent_sym = make_dir_symbol(ctx->name, folder->parent->path);
        if (parent_sym) {
            writer_printf(ctx->w, ".parent = &%s,\n", parent_sym);
            free(parent_sym);
        }
    } else {
        writer_puts(ctx->w, ".parent = NULL,\n");
    }

    /* Children - point to first child using path-based name */
    if (folder->children) {
        char *child_sym = make_dir_symbol(ctx->name, folder->children->path);
        if (child_sym) {
            writer_printf(ctx->w, ".children = &%s,\n", child_sym);
            free(child_sym);
        }
        writer_printf(ctx->w, ".child_count = %d,\n", info->children_count);
    } else {
        writer_puts(ctx->w, ".children = NULL,\n");
        writer_puts(ctx->w, ".child_count = 0,\n");
    }

    /* Files - use path-based array name */
    if (info->files_count > 0) {
        char *files_sym = make_dir_symbol(ctx->name, folder->path);
        if (files_sym) {
            writer_printf(ctx->w, ".files = %s_files,\n", files_sym);
            free(files_sym);
        }
        writer_printf(ctx->w, ".file_count = %d,\n", info->files_count);
    } else {
        writer_puts(ctx->w, ".files = NULL,\n");
        writer_puts(ctx->w, ".file_count = 0,\n");
    }

    /* Metadata */
    if (info->metadata_index >= 0) {
        writer_printf(ctx->w, ".metadata = %s_meta_%d,\n", ctx->name, info->metadata_index);
        writer_printf(ctx->w, ".metadata_count = %zu\n", vfs_metadata_count(folder->metadata));
    } else {
        writer_puts(ctx->w, ".metadata = NULL,\n");
        writer_puts(ctx->w, ".metadata_count = 0\n");
    }

    writer_dedent(ctx->w);
    writer_printf(ctx->w, "};\n\n");
}

static void generate_all_files_arrays(codegen_ctx_t *ctx, const vfs_folder_t *folder,
                                       folder_info_t *info_list, file_meta_info_t *file_meta_list,
                                       int *file_idx)
{
    generate_files_array(ctx, folder, info_list, file_meta_list, file_idx);

    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        generate_all_files_arrays(ctx, c, info_list, file_meta_list, file_idx);
    }
}

static void generate_all_folders(codegen_ctx_t *ctx, const vfs_folder_t *folder,
                                  folder_info_t *info_list)
{
    /* Generate children first (they need to be defined before parent references them) */
    for (const vfs_folder_t *c = folder->children; c; c = c->next) {
        generate_all_folders(ctx, c, info_list);
    }

    /* Generate this folder */
    generate_folder_struct(ctx, folder, info_list);
}

static cirf_error_t generate_header(const cirf_config_t *config, const char *path)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return CIRF_ERR_IO;

    writer_t *w = writer_create(fp);
    if (!w) {
        fclose(fp);
        return CIRF_ERR_NOMEM;
    }

    const char *name = config->name;

    /* Header guard */
    char *guard = make_identifier(name);
    for (char *p = guard; *p; p++) *p = toupper((unsigned char)*p);

    writer_printf(w, "#ifndef %s_H\n", guard);
    writer_printf(w, "#define %s_H\n\n", guard);
    free(guard);

    /* Include common types - use cirf_file_t, cirf_folder_t, cirf_metadata_t */
    writer_puts(w, "#include <cirf/types.h>\n\n");

    /* Root declaration */
    writer_printf(w, "extern const cirf_folder_t %s_root;\n", name);

    /* Folder declarations */
    generate_folder_extern_decls(w, name, config->root);
    writer_newline(w);

    /* File declarations */
    generate_file_extern_decls(w, name, config->root);

    writer_printf(w, "\n#endif /* %s_H */\n", name);

    writer_destroy(w);
    fclose(fp);
    return CIRF_OK;
}

static cirf_error_t generate_source(const cirf_config_t *config, const char *path,
                                     const char *header_name)
{
    FILE *fp = fopen(path, "w");
    if (!fp) return CIRF_ERR_IO;

    writer_t *w = writer_create(fp);
    if (!w) {
        fclose(fp);
        return CIRF_ERR_NOMEM;
    }

    const char *name = config->name;

    writer_printf(w, "#include \"%s\"\n\n", header_name);

    codegen_ctx_t ctx = {
        .name = name,
        .w = w,
        .file_index = 0,
        .folder_index = 0,
        .metadata_index = 0
    };

    /* Generate all file data arrays */
    generate_all_data(&ctx, config->root);

    /* Collect folder info for cross-references */
    folder_info_t *info_list = NULL;
    int file_idx = 0;
    int folder_idx = 0;
    collect_folder_info(config->root, &info_list, &file_idx, &folder_idx);

    /* Forward declarations for all folders (except root) */
    for (folder_info_t *info = info_list; info; info = info->next) {
        if (info->self_index > 0) { /* Skip root */
            generate_folder_forward_decl(&ctx, info->folder);
        }
    }
    writer_newline(w);

    /* Generate all file metadata arrays first */
    file_meta_info_t *file_meta_list = NULL;
    generate_all_file_metadata(&ctx, config->root, &file_meta_list);

    /* Generate files arrays */
    file_idx = 0;
    generate_all_files_arrays(&ctx, config->root, info_list, file_meta_list, &file_idx);

    /* Generate folder structures (children before parents) */
    generate_all_folders(&ctx, config->root, info_list);

    free_file_meta_info(file_meta_list);
    free_folder_info(info_list);

    /* No API implementations - use cirf_runtime library for helper functions */

    writer_destroy(w);
    fclose(fp);
    return CIRF_OK;
}

cirf_error_t codegen_generate(const cirf_config_t *config,
                               const codegen_options_t *options)
{
    if (!config || !options || !options->source_path || !options->header_path) {
        return CIRF_ERR_INVALID;
    }

    cirf_error_t err = generate_header(config, options->header_path);
    if (err != CIRF_OK) {
        return err;
    }

    /* Extract header filename for #include */
    const char *header_name = strrchr(options->header_path, '/');
    if (header_name) {
        header_name++;
    } else {
        header_name = options->header_path;
    }

    return generate_source(config, options->source_path, header_name);
}
