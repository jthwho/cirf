# CIRF ESP32 Example

This example demonstrates embedding web resources into an ESP32 application using CIRF.

## Features

- **Automatic host tool building** - The cirf executable is built for your host machine automatically
- **Cross-compilation support** - Runtime library is compiled for ESP32
- **Build-time generation** - Resources are generated during the build process
- **Dependency tracking** - Changes to source files trigger regeneration

## Prerequisites

1. **ESP-IDF** installed and configured (v4.4+ recommended)
2. **Host C compiler** (gcc, clang) - for building the cirf tool

## Building

```bash
# Set up ESP-IDF environment
source /path/to/esp-idf/export.sh

# Navigate to example
cd /path/to/cirf/examples/esp32

# Build (cirf tool and resources are generated automatically)
idf.py build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

That's it! The build system will:
1. Build the `cirf` executable for your host machine (using gcc/clang)
2. Generate `web_resources.c` and `web_resources.h` from `resources.json`
3. Compile the runtime library for ESP32
4. Link everything together

### Using a Pre-built cirf

If you prefer to use a pre-built cirf (or have it installed system-wide):

```bash
# Option 1: Install system-wide (then it's found automatically)
cd /path/to/cirf && mkdir build && cd build
cmake .. && make cirf && sudo make install

# Option 2: Specify path explicitly
idf.py build -DCIRF_EXECUTABLE=/path/to/cirf
```

## Project Structure

```
esp32/
├── CMakeLists.txt          # ESP-IDF project file
├── sdkconfig.defaults      # CIRF settings for ESP32
├── resources.json          # CIRF resource configuration
├── README.md
└── main/
    ├── CMakeLists.txt      # Uses cirf_generate() macro
    ├── main.c              # Application code
    └── resources/          # Files to embed
        ├── index.html
        ├── css/style.css
        └── api/config.json

# Build outputs:
build/
├── cirf-host/              # Host-built cirf tool
│   └── cirf
└── esp-idf/main/           # Generated resources
    ├── web_resources.c
    └── web_resources.h
```

## Usage in Code

### Direct Symbol Access (No Runtime)

```c
#include "web_resources.h"

// Access files directly by generated symbol
printf("Size: %zu\n", web_resources_file_index_html->size);
printf("MIME: %s\n", web_resources_file_index_html->mime);

// Send data (e.g., via HTTP)
httpd_resp_send(req, (const char *)web_resources_file_index_html->data,
                web_resources_file_index_html->size);
```

### Runtime Library Functions

```c
#include "web_resources.h"
#include <cirf/runtime.h>

// Look up file by path (e.g., from HTTP request URI)
const cirf_file_t *file = cirf_find_file(&web_resources_root, "css/style.css");
if (file) {
    httpd_resp_set_type(req, file->mime);
    httpd_resp_send(req, (const char *)file->data, file->size);
}
```

## Configuration Options

In `menuconfig` under "CIRF Runtime Configuration":

| Option | Default | Description |
|--------|---------|-------------|
| `CIRF_MAX_PATH` | 128 | Maximum path length for lookups |
| `CIRF_NO_STDIO` | yes | Disable FILE* functions (fmemopen unavailable) |
| `CIRF_NO_MOUNT` | yes | Disable mount system (avoids malloc) |

## Memory Usage

The embedded resources are stored in flash (rodata section) and accessed directly without copying to RAM. Only the path lookup functions use a small amount of stack space.

Typical overhead:
- Per file: ~40 bytes metadata + file size
- Per folder: ~36 bytes
- Runtime library: ~1KB code

## Adding More Resources

1. Add files to `main/resources/`
2. Update `resources.json` with new entries
3. Add the new files to the `DEPENDS` list in `main/CMakeLists.txt`
4. Rebuild - resources regenerate automatically

## Using in Your Own Project

### 1. Add CIRF as a component

Copy or symlink the `idf_component` directory to your project's `components/` folder:

```bash
cd your_project/components
ln -s /path/to/cirf/idf_component cirf
```

Or add to your project's `CMakeLists.txt`:

```cmake
set(EXTRA_COMPONENT_DIRS "/path/to/cirf/idf_component")
```

### 2. Use cirf_generate() in your component

In your component's `CMakeLists.txt`:

```cmake
# Generate resources from config file
cirf_generate(
    NAME my_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../my_resources.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/files/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/files/app.js
    OUTPUT_SOURCES MY_RESOURCE_SOURCES
)

idf_component_register(
    SRCS
        "main.c"
        ${MY_RESOURCE_SOURCES}
    INCLUDE_DIRS
        "."
        "${MY_RESOURCE_SOURCES_INCLUDE_DIR}"
    REQUIRES
        cirf
)
```

### 3. Include and use

```c
#include "my_resources.h"
#include <cirf/runtime.h>

// Direct access
const uint8_t *data = my_resources_file_index_html->data;

// Or runtime lookup
const cirf_file_t *f = cirf_find_file(&my_resources_root, uri);
```

### Using Glob Patterns

For many files, use glob patterns in `resources.json`:

```json
{
    "entries": [
        {
            "type": "glob",
            "pattern": "./main/resources/images/*.png",
            "target": "images/"
        }
    ]
}
```

## Integrating with HTTP Server

Example with ESP-IDF HTTP Server:

```c
static esp_err_t file_handler(httpd_req_t *req)
{
    // Get path from URI (skip leading '/')
    const char *path = req->uri + 1;
    if (path[0] == '\0') path = "index.html";

    const cirf_file_t *file = cirf_find_file(&web_resources_root, path);
    if (!file) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    httpd_resp_set_type(req, file->mime);
    httpd_resp_send(req, (const char *)file->data, file->size);
    return ESP_OK;
}
```
