# CIRF - C Inline Resource Filesystem

CIRF is a C99 utility that compiles arbitrary files into C source code as embedded virtual filesystem structures. It generates nested C structures representing directories and files, allowing you to bundle resources directly into your executables without external dependencies.

## Features

- **Pure C99**: No external dependencies beyond the C standard library
- **Virtual Filesystem**: Generates nested folder/file structures in C
- **Bidirectional Traversal**: Both parent and child references for easy navigation
- **MIME Type Detection**: Automatic MIME type guessing based on file extensions
- **Metadata Support**: Key/value metadata for both files and folders
- **Multiple VFS Support**: Custom base names allow multiple filesystems per project
- **Common Types**: Shared type definitions (`cirf/types.h`) allow multiple resource sets to interoperate
- **Optional Runtime Library**: Helper functions for path lookups, iteration, and FILE* integration
- **Cross-Compilation**: Full support for embedded targets (ESP32, ARM, etc.)
- **CMake Integration**: Easy integration into existing CMake projects
- **ESP-IDF Component**: First-class support for ESP32 development

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Usage

```bash
cirf -n <basename> -c <config.json> -o <output.c> -H <output.h>
```

### Options

| Option | Description |
|--------|-------------|
| `-n, --name <name>` | Base name for all generated symbols (required) |
| `-c, --config <file>` | Input configuration file (JSON) |
| `-o, --output <file>` | Output C source file |
| `-H, --header <file>` | Output C header file |
| `-d, --deps` | Output source file dependencies (one per line) |
| `-M, --depfile <file>` | Write Makefile-format dependency file |
| `--help` | Show help message |
| `--version` | Show version information |

### Example

```bash
cirf -n game_assets -c assets.json -o game_assets.c -H game_assets.h
```

This generates:
- **Header**: Includes `<cirf/types.h>` for common type definitions
- **Root symbol**: `game_assets_root` (type: `cirf_folder_t`)
- **File symbols**: `game_assets_file_icon_png`, etc. (type: `cirf_file_t*`)
- **Folder symbols**: `game_assets_dir_images`, etc. (type: `cirf_folder_t`)

## Configuration File Format

CIRF uses a JSON configuration file to describe the virtual filesystem:

```json
{
    "metadata": {
        "version": "1.0.0",
        "author": "Your Name"
    },
    "entries": [
        {
            "type": "file",
            "path": "images/logo.png",
            "source": "./resources/logo.png",
            "metadata": {
                "description": "Application logo"
            }
        },
        {
            "type": "file",
            "path": "config/defaults.json",
            "source": "./resources/defaults.json",
            "mime": "application/json"
        },
        {
            "type": "folder",
            "path": "data",
            "metadata": {
                "category": "runtime-data"
            },
            "entries": [
                {
                    "type": "file",
                    "path": "readme.txt",
                    "source": "./resources/data/readme.txt"
                }
            ]
        },
        {
            "type": "glob",
            "pattern": "./resources/shaders/*.glsl",
            "target": "shaders/"
        }
    ]
}
```

### Configuration Fields

| Field | Type | Description |
|-------|------|-------------|
| `metadata` | object | Key/value metadata for root folder |
| `entries` | array | Array of file/folder/glob entries |

### Entry Types

**File Entry:**
- `type`: `"file"`
- `path`: Virtual path within the filesystem
- `source`: Path to the actual file on disk
- `mime`: (optional) MIME type override
- `metadata`: (optional) Key/value metadata

**Folder Entry:**
- `type`: `"folder"`
- `path`: Virtual path for the folder
- `metadata`: (optional) Key/value metadata
- `entries`: (optional) Child entries

**Glob Entry:**
- `type`: `"glob"`
- `pattern`: File glob pattern
- `target`: Target virtual directory

## Generated Code Structure

CIRF uses common types defined in `<cirf/types.h>`. Generated headers include this file and declare symbols with resource-specific prefixes. Given `-n myres`:

```c
/* In myres.h */
#include <cirf/types.h>

/* Root folder */
extern const cirf_folder_t myres_root;

/* Folders exposed by path-derived names */
extern const cirf_folder_t myres_dir_images;
extern const cirf_folder_t myres_dir_images_icons;

/* Files exposed as pointers by path-derived names */
extern const cirf_file_t * const myres_file_readme_txt;
extern const cirf_file_t * const myres_file_images_logo_png;
extern const cirf_file_t * const myres_file_images_icons_app_png;
```

### Common Types (`cirf/types.h`)

All generated resources use these shared types:

```c
typedef struct cirf_metadata {
    const char *key;
    const char *value;
} cirf_metadata_t;

typedef struct cirf_file {
    const char *name;
    const char *path;               /* Full virtual path */
    const char *mime;
    const unsigned char *data;
    size_t size;
    const cirf_folder_t *parent;    /* Parent folder */
    const cirf_metadata_t *metadata;
    size_t metadata_count;
} cirf_file_t;

typedef struct cirf_folder {
    const char *name;
    const char *path;               /* Full virtual path */
    const cirf_folder_t *parent;    /* Parent folder (NULL for root) */
    const cirf_folder_t *children;  /* Child folders */
    size_t child_count;
    const cirf_file_t *files;       /* Files in this folder */
    size_t file_count;
    const cirf_metadata_t *metadata;
    size_t metadata_count;
} cirf_folder_t;
```

Using common types means multiple resource sets can interoperate:

```c
#include "game_textures.h"
#include "game_sounds.h"

/* Both use the same cirf_file_t type */
void process_file(const cirf_file_t *file) {
    printf("%s: %zu bytes\n", file->path, file->size);
}

process_file(game_textures_file_player_png);
process_file(game_sounds_file_explosion_wav);
```

### Direct Access

All files and folders are exposed with path-derived symbol names, allowing direct access without runtime lookups:

```c
/* Direct access to files (no lookup needed) */
const unsigned char *data = myres_file_readme_txt->data;
size_t size = myres_file_readme_txt->size;

/* Direct access to folders */
size_t file_count = myres_dir_images.file_count;

/* Naming convention:
 * - Folders: {name}_dir_{path_with_underscores}
 * - Files:   {name}_file_{path_with_underscores}
 * - Root:    {name}_root
 */
```

### Bidirectional Traversal

The parent pointers allow you to traverse up the tree:

```c
/* Walk up to root from any file */
const cirf_file_t *file = cirf_find_file(&myres_root, "images/icons/app.png");
if (file) {
    const cirf_folder_t *folder = file->parent;
    while (folder) {
        printf("In folder: %s\n", folder->name);
        folder = folder->parent;
    }
}

/* Get full path of parent */
const cirf_folder_t *parent = file->parent;
printf("Parent path: %s\n", parent->path);  /* "images/icons" */
```

## Runtime Library

The optional `cirf_runtime` library provides helper functions for working with generated resources. Link against it when you need path-based lookups, iteration, or FILE* integration.

```c
#include <cirf/runtime.h>
#include "myres.h"

/* Find a file by path */
const cirf_file_t *file = cirf_find_file(&myres_root, "images/logo.png");

/* Find a folder by path */
const cirf_folder_t *folder = cirf_find_folder(&myres_root, "images");

/* Get metadata value by key */
const char *version = cirf_get_metadata(myres_root.metadata,
                                         myres_root.metadata_count, "version");

/* Iterate all files recursively */
void print_file(const cirf_file_t *f, void *ctx) {
    printf("  %s\n", f->path);
}
cirf_foreach_file_recursive(&myres_root, print_file, NULL);

/* FILE* integration (POSIX systems) */
FILE *fp = cirf_fopen(myres_file_config_json);
if (fp) {
    char buf[256];
    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
    }
    fclose(fp);
}
```

### Runtime Configuration

For embedded systems, the runtime can be configured to reduce footprint:

| Option | Effect |
|--------|--------|
| `CIRF_NO_STDIO` | Disable FILE* functions (no fmemopen dependency) |
| `CIRF_NO_MOUNT` | Disable mount system (no malloc dependency) |
| `CIRF_MAX_PATH` | Maximum path length for lookups (default: 256) |

## Multiple Virtual Filesystems

You can include multiple VFS in the same project by using different base names:

```bash
cirf -n game_textures -c textures.json -o game_textures.c -H game_textures.h
cirf -n game_sounds -c sounds.json -o game_sounds.c -H game_sounds.h
cirf -n game_levels -c levels.json -o game_levels.c -H game_levels.h
```

Each generates independent symbols that share the common types:

```c
#include "game_textures.h"
#include "game_sounds.h"
#include "game_levels.h"
#include <cirf/runtime.h>

/* All use cirf_find_file() with their respective roots */
const cirf_file_t *tex = cirf_find_file(&game_textures_root, "player.png");
const cirf_file_t *snd = cirf_find_file(&game_sounds_root, "explosion.wav");
const cirf_file_t *lvl = cirf_find_file(&game_levels_root, "level1.dat");
```

## CMake Integration

### As a Subdirectory

```cmake
add_subdirectory(cirf)

# Without runtime library (direct access only)
cirf_add_resources(game_assets
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
)

# With runtime library (helper functions available)
cirf_add_resources(game_assets
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    LINK_RUNTIME
)

target_link_libraries(my_app PRIVATE game_assets)
```

### Using find_package

```cmake
find_package(CIRF REQUIRED)

cirf_add_resources(game_assets
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    LINK_RUNTIME
)

target_link_libraries(my_app PRIVATE game_assets)
```

### Using FetchContent (External Package)

CIRF can be included directly in your project using CMake's FetchContent:

```cmake
include(FetchContent)

FetchContent_Declare(
    cirf
    GIT_REPOSITORY https://github.com/jthwho/cirf.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(cirf)

# Use cirf_add_resources as normal
cirf_add_resources(game_assets
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    LINK_RUNTIME
)

target_link_libraries(my_app PRIVATE game_assets)
```

This approach automatically downloads and builds CIRF as part of your project's configure step, requiring no pre-installation.

### CMake Function Options

| Option | Description |
|--------|-------------|
| `CONFIG` | Path to configuration JSON file (required) |
| `OUTPUT_DIR` | Directory for generated files (default: `CMAKE_CURRENT_BINARY_DIR`) |
| `LINK_RUNTIME` | Link against `cirf_runtime` for helper functions |
| `CIRF_EXECUTABLE` | Path to cirf executable (for cross-compilation) |

The first argument to `cirf_add_resources` is the base name, which determines:
- The CMake target name
- All generated symbol prefixes
- Output file names

### Cross-Compilation

For embedded targets, use `cirf_generate_resources()` from the cross-compilation module:

```cmake
include(CIRF)

# Generate resources (cirf tool built automatically, dependencies auto-detected)
cirf_generate_resources(
    NAME my_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    OUTPUT_VAR MY_RESOURCE_SOURCES
)

# Add runtime library for target platform
cirf_add_runtime_library()

# Use in your target
add_executable(my_app main.c ${MY_RESOURCE_SOURCES})
target_include_directories(my_app PRIVATE ${MY_RESOURCE_SOURCES_INCLUDE_DIR})
target_link_libraries(my_app PRIVATE cirf_runtime)
```

The cirf tool is built automatically from source when needed. Source file dependencies
are tracked at build time using the `--depfile` option, so modifying any source file
will trigger regeneration of the resources.

## ESP-IDF Integration

CIRF includes an ESP-IDF component for seamless ESP32 development.

### Setup

Add CIRF as a component:

```bash
cd your_project/components
ln -s /path/to/cirf/idf_component cirf
```

Or in your project's `CMakeLists.txt`:

```cmake
set(EXTRA_COMPONENT_DIRS "/path/to/cirf/idf_component")
```

### Usage

In your component's `CMakeLists.txt`:

```cmake
cirf_generate(
    NAME web_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../resources.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/files/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/files/style.css
    OUTPUT_SOURCES WEB_RESOURCES_SOURCES
)

idf_component_register(
    SRCS "main.c" ${WEB_RESOURCES_SOURCES}
    INCLUDE_DIRS "." "${WEB_RESOURCES_SOURCES_INCLUDE_DIR}"
    REQUIRES cirf
)
```

The build system automatically:
1. Builds the `cirf` executable for your host machine
2. Generates resources at build time
3. Compiles the runtime library for ESP32

### Configuration

In `menuconfig` under "CIRF Runtime Configuration":

| Option | Default | Description |
|--------|---------|-------------|
| `CIRF_MAX_PATH` | 128 | Maximum path length for lookups |
| `CIRF_NO_STDIO` | yes | Disable FILE* functions |
| `CIRF_NO_MOUNT` | yes | Disable mount system |

See `examples/esp32/` for a complete example.

## Examples

The `examples/` directory contains standalone example projects:

- **simple/**: Basic usage with direct access and runtime library
- **esp32/**: ESP-IDF integration for embedded web server

Examples are standalone projects to demonstrate real-world usage:

```bash
cd examples/simple
mkdir build && cd build
cmake ..
make
./simple_example
```

## Architecture

CIRF follows object-oriented design principles in C:

- **Modular Design**: Separate compilation units for parsing, generation, and filesystem operations
- **Opaque Types**: Implementation details hidden behind clean interfaces
- **Error Handling**: Consistent error reporting through return codes and error callbacks
- **Memory Safety**: Clear ownership semantics and cleanup functions

## License

MIT License - See LICENSE file for details.

## Contributing

Contributions are welcome! Please read CONTRIBUTING.md for guidelines.
