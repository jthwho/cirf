# CIRF Architecture

This document describes the internal architecture of CIRF, following object-oriented design principles in C similar to the Linux kernel style.

## Design Principles

### SOLID in C

1. **Single Responsibility**: Each module handles one concern
   - `json.c` - JSON parsing only
   - `mime.c` - MIME type detection only
   - `vfs.c` - Virtual filesystem tree management
   - `codegen.c` - C code generation only

2. **Open/Closed**: Extensible through function pointers and callbacks
   - Error handlers are injectable
   - Output writers use a common interface

3. **Liskov Substitution**: Consistent interfaces for similar types
   - All nodes (files/folders) share common traversal patterns

4. **Interface Segregation**: Small, focused headers
   - Each module exposes only what's needed

5. **Dependency Inversion**: High-level modules don't depend on low-level details
   - Code generator depends on abstract VFS, not JSON structure

### Memory Management

- **Ownership**: Each structure documents who owns allocated memory
- **Cleanup Functions**: Every `*_create()` has a corresponding `*_destroy()`
- **Arena-style allocation**: Where appropriate, allocate in chunks for efficiency

### Error Handling

All functions that can fail follow this pattern:

```c
typedef enum {
    CIRF_OK = 0,
    CIRF_ERR_NOMEM,
    CIRF_ERR_IO,
    CIRF_ERR_PARSE,
    CIRF_ERR_INVALID,
    /* ... */
} cirf_error_t;

/* Functions return error code */
cirf_error_t cirf_do_something(args...);

/* Error context available via thread-local or passed context */
const char *cirf_error_message(cirf_error_t err);
```

## Module Overview

### Code Generator

```
┌─────────────────────────────────────────────────────────────┐
│                          main.c                              │
│                   (CLI argument parsing)                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                         config.c                             │
│              (Configuration loading & validation)            │
└─────────────────────────────────────────────────────────────┘
          │                   │                    │
          ▼                   ▼                    ▼
┌─────────────┐    ┌──────────────────┐    ┌─────────────┐
│   json.c    │    │      vfs.c       │    │   glob.c    │
│  (Parser)   │    │ (Tree structure) │    │  (Pattern   │
│             │    │                  │    │   matching) │
└─────────────┘    └──────────────────┘    └─────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        codegen.c                             │
│                (C source code generation)                    │
└─────────────────────────────────────────────────────────────┘
          │                                        │
          ▼                                        ▼
┌─────────────────┐                    ┌─────────────────┐
│    writer.c     │                    │     mime.c      │
│ (Output buffer  │                    │  (MIME type     │
│   management)   │                    │   detection)    │
└─────────────────┘                    └─────────────────┘
```

### Runtime Library (Optional)

```
┌─────────────────────────────────────────────────────────────┐
│                    include/cirf/types.h                      │
│              (Common type definitions - standalone)          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   include/cirf/runtime.h                     │
│                     (Runtime API header)                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      src/runtime.c                           │
│    (Helper functions: find, iterate, FILE* integration)      │
└─────────────────────────────────────────────────────────────┘
```

The types header (`cirf/types.h`) is standalone and has no dependencies. Generated headers include only this file. The runtime header (`cirf/runtime.h`) includes the types header and is needed only when linking the runtime library.

## Module Details

### json.c / json.h

Minimal JSON parser supporting the subset needed for configuration:

- Objects (key-value pairs)
- Arrays
- Strings
- Numbers (integers only)
- Booleans
- Null

**Key Types:**
```c
typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} json_type_t;

typedef struct json_value json_value_t;

struct json_value {
    json_type_t type;
    union {
        int boolean;
        long number;
        char *string;
        struct {
            json_value_t *items;
            size_t count;
        } array;
        struct {
            char **keys;
            json_value_t *values;
            size_t count;
        } object;
    } data;
};
```

**Key Functions:**
```c
cirf_error_t json_parse(const char *input, json_value_t **out);
void json_destroy(json_value_t *value);
json_value_t *json_get(json_value_t *obj, const char *key);
json_value_t *json_array_get(json_value_t *arr, size_t index);
```

### vfs.c / vfs.h

In-memory representation of the virtual filesystem tree.

**Key Types:**
```c
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
    struct vfs_file *next;      /* Sibling list */
} vfs_file_t;

typedef struct vfs_folder {
    char *name;
    char *path;
    vfs_metadata_t *metadata;
    struct vfs_folder *parent;
    struct vfs_folder *children;
    struct vfs_folder *next;    /* Sibling list */
    vfs_file_t *files;
} vfs_folder_t;
```

**Key Functions:**
```c
vfs_folder_t *vfs_create_root(void);
void vfs_destroy(vfs_folder_t *root);
vfs_folder_t *vfs_add_folder(vfs_folder_t *parent, const char *name);
vfs_file_t *vfs_add_file(vfs_folder_t *parent, const char *name,
                          const char *source_path);
cirf_error_t vfs_load_file_data(vfs_file_t *file);
void vfs_add_metadata(vfs_metadata_t **list, const char *key, const char *value);
```

### mime.c / mime.h

MIME type detection based on file extensions.

**Key Functions:**
```c
const char *mime_from_extension(const char *filename);
const char *mime_from_path(const char *path);
```

Uses a static lookup table of common extensions. Falls back to `application/octet-stream` for unknown types.

### glob.c / glob.h

Simple glob pattern matching for file discovery.

**Supported Patterns:**
- `*` - Match any characters except `/`
- `?` - Match single character
- `**` - Match any path segments

**Key Functions:**
```c
typedef int (*glob_callback_t)(const char *path, void *ctx);

cirf_error_t glob_match(const char *pattern, const char *base_dir,
                         glob_callback_t callback, void *ctx);
int glob_pattern_match(const char *pattern, const char *string);
```

### config.c / config.h

Loads JSON configuration and builds VFS tree.

**Key Functions:**
```c
typedef struct cirf_config {
    char *name;          /* Base name for generated symbols */
    vfs_folder_t *root;  /* Built filesystem tree */
} cirf_config_t;

cirf_error_t config_load(const char *path, const char *name, cirf_config_t **out);
void config_destroy(cirf_config_t *config);
```

### codegen.c / codegen.h

Generates C source and header files from VFS tree.

**Key Functions:**
```c
typedef struct codegen_options {
    const char *name;           /* Base name for symbols */
    const char *source_path;    /* Output .c path */
    const char *header_path;    /* Output .h path */
} codegen_options_t;

cirf_error_t codegen_generate(const cirf_config_t *config,
                               const codegen_options_t *options);
```

### writer.c / writer.h

Buffered output with formatting helpers.

**Key Types:**
```c
typedef struct writer writer_t;

writer_t *writer_create(FILE *fp);
void writer_destroy(writer_t *w);
void writer_printf(writer_t *w, const char *fmt, ...);
void writer_indent(writer_t *w);
void writer_dedent(writer_t *w);
void writer_write_bytes(writer_t *w, const unsigned char *data, size_t len);
```

## Generated Code Structure

The code generator produces structures optimized for const-correctness, direct access, and minimal runtime overhead. All generated code uses the common types from `<cirf/types.h>`.

### Symbol Naming Convention

All generated symbols use path-derived names for direct access:

| Type | Pattern | Example |
|------|---------|---------|
| Root folder | `{name}_root` | `myres_root` |
| Subfolder | `{name}_dir_{path}` | `myres_dir_images`, `myres_dir_images_icons` |
| File pointer | `{name}_file_{path}` | `myres_file_readme_txt`, `myres_file_images_logo_png` |

Path separators (`/`) and special characters become underscores in symbol names.

### Header File Structure

```c
#ifndef {NAME}_H
#define {NAME}_H

#include <cirf/types.h>

/* Root folder */
extern const cirf_folder_t {name}_root;

/* All folders exposed by path-derived names */
extern const cirf_folder_t {name}_dir_images;
extern const cirf_folder_t {name}_dir_images_icons;
extern const cirf_folder_t {name}_dir_config;

/* All files exposed as const pointers by path-derived names */
extern const cirf_file_t * const {name}_file_readme_txt;
extern const cirf_file_t * const {name}_file_images_logo_png;
extern const cirf_file_t * const {name}_file_config_settings_json;

#endif
```

Note: API functions like `cirf_find_file()` are provided by the optional `cirf_runtime` library, not generated per-resource.

### Source File Structure

```c
#include "{name}.h"

/* File data arrays (static, indexed) */
static const unsigned char {name}_data_0[] = { ... };
static const unsigned char {name}_data_1[] = { ... };

/* Metadata arrays */
static const cirf_metadata_t {name}_meta_0[] = { ... };

/* Forward declarations for folders */
const cirf_folder_t {name}_dir_config;

/* Files array for root folder */
static const cirf_file_t {name}_root_files[] = {
    { .name = "readme.txt", .path = "readme.txt", ... }
};

/* Exposed file pointers (point into arrays) */
const cirf_file_t * const {name}_file_readme_txt = &{name}_root_files[0];

/* Files array for config folder */
static const cirf_file_t {name}_dir_config_files[] = {
    { .name = "settings.json", .path = "config/settings.json", ... }
};

const cirf_file_t * const {name}_file_config_settings_json = &{name}_dir_config_files[0];

/* Folder definitions (children before parents for forward references) */
const cirf_folder_t {name}_dir_config = {
    .name = "config",
    .path = "config",
    .parent = &{name}_root,
    .files = {name}_dir_config_files,
    ...
};

/* Root folder */
const cirf_folder_t {name}_root = {
    .name = "",
    .path = "",
    .parent = NULL,
    .children = &{name}_dir_config,
    .files = {name}_root_files,
    ...
};
```

### Direct Access vs Path Lookup

The generated code supports two access patterns:

**Direct Access (compile-time, no runtime library needed):**
```c
/* No runtime lookup - symbol resolved at link time */
const unsigned char *data = myres_file_config_settings_json->data;
size_t count = myres_dir_images.file_count;
```

**Path Lookup (runtime, requires cirf_runtime library):**
```c
#include <cirf/runtime.h>

/* Dynamic path resolution */
const cirf_file_t *f = cirf_find_file(&myres_root, user_path);
```

Direct access is more efficient when the path is known at compile time.

## Runtime Library

The `cirf_runtime` library (`src/runtime.c`) provides optional helper functions for working with generated resources. It is designed to be embedded-friendly with configurable features.

### Features

| Function | Description |
|----------|-------------|
| `cirf_find_file()` | Find file by path |
| `cirf_find_folder()` | Find folder by path |
| `cirf_get_metadata()` | Get metadata value by key |
| `cirf_foreach_file()` | Iterate files in folder |
| `cirf_foreach_file_recursive()` | Iterate files recursively |
| `cirf_count_files()` | Count files in tree |
| `cirf_fopen()` | Open file as FILE* (POSIX) |
| `cirf_mount()` | Mount resources under prefix |

### Configuration

For embedded targets, configure with compile definitions:

| Option | Effect |
|--------|--------|
| `CIRF_NO_STDIO` | Removes FILE* functions (no fmemopen dependency) |
| `CIRF_NO_MOUNT` | Removes mount system (no malloc dependency) |
| `CIRF_MAX_PATH` | Maximum path length (default: 256, use 128 for embedded) |

### Memory Model

- **No heap allocation** (with `CIRF_NO_MOUNT`): Uses only stack for path buffers
- **Configurable stack usage**: `CIRF_MAX_PATH` controls buffer sizes
- **Const-correct**: All functions work with const pointers to generated data

## Build Integration

### CMake Functions

CIRF provides two CMake integration approaches:

#### cirf_add_resources() - Standard Builds

For native builds or when cirf is pre-installed:

```cmake
cirf_add_resources(my_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    LINK_RUNTIME  # Optional: link cirf_runtime for helper functions
)
```

This function:
1. Creates a custom command to run `cirf`
2. Sets up proper dependencies on input files
3. Creates a static library target from generated sources
4. Sets up include directories for `<cirf/types.h>`
5. Optionally links `cirf_runtime`

#### cirf_generate_resources() - Cross-Compilation

For embedded targets (ESP32, ARM, etc.):

```cmake
include(CIRFGenerateResources)

# Build cirf for host automatically if needed
cirf_ensure_host_tool()

# Generate resources (returns source files in variable)
cirf_generate_resources(
    NAME my_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    OUTPUT_VAR MY_SOURCES
)

# Add runtime library compiled for target
cirf_add_runtime_library()

# Use in your target
add_executable(app main.c ${MY_SOURCES})
target_link_libraries(app cirf_runtime)
```

### ESP-IDF Component

For ESP32 projects, the `idf_component/` provides:

```cmake
cirf_generate(
    NAME web_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/files/index.html
    OUTPUT_SOURCES MY_SOURCES
)
```

This automatically:
1. Builds cirf for the host machine
2. Generates resources at build time
3. Provides sources via `OUTPUT_SOURCES` variable
4. Builds runtime library for ESP32

## Testing Strategy

1. **Unit Tests**: Each module has isolated tests
   - JSON parser edge cases
   - MIME detection coverage
   - Glob pattern matching

2. **Integration Tests**: End-to-end generation tests
   - Generate from sample configs
   - Compile generated code
   - Verify runtime behavior

3. **Fuzz Testing**: Parser robustness
   - Malformed JSON handling
   - Large file handling
