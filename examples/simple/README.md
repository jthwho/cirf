# CIRF Simple Example

This example demonstrates how to use CIRF in a standalone project.

## Features Demonstrated

- **Direct symbol access**: Access embedded files via generated symbols (no runtime needed)
- **Runtime library functions**: Use `cirf_find_file()`, `cirf_foreach_file_recursive()`, etc.
- **FILE* integration**: Use `cirf_fopen()` for standard C file I/O compatibility
- **Metadata access**: Read key-value metadata from files and folders

## Building

```bash
cd examples/simple
mkdir build && cd build
cmake ..
make
./simple_example
```

## Project Structure

```
simple/
├── CMakeLists.txt      # Build configuration (demonstrates CIRF integration)
├── main.c              # Example application
├── resources.json      # CIRF resource configuration
└── resources/          # Files to embed
    ├── hello.txt
    └── data.json
```

## How It Works

### 1. Set CIRF Source Directory

```cmake
# Point to CIRF source (adjust path for your project)
set(CIRF_SOURCE_DIR "/path/to/cirf")
```

### 2. Include CIRF CMake Module

```cmake
list(APPEND CMAKE_MODULE_PATH "${CIRF_SOURCE_DIR}/cmake")
include(CIRFGenerateResources)
```

### 3. Build Host Tool and Generate Resources

```cmake
cirf_ensure_host_tool()

cirf_generate_resources(
    NAME my_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    OUTPUT_VAR RESOURCE_SOURCES
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/file1.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/file2.json
)
```

### 4. Add Runtime Library (Optional)

```cmake
cirf_add_runtime_library()
```

### 5. Use in Your Target

```cmake
add_executable(my_app main.c ${RESOURCE_SOURCES})
target_include_directories(my_app PRIVATE ${RESOURCE_SOURCES_INCLUDE_DIR})
target_link_libraries(my_app PRIVATE cirf_runtime)
```

## Usage in Code

### Direct Access (No Runtime Library)

```c
#include "my_resources.h"

// Access via generated symbols
const uint8_t *data = my_resources_file_config_json->data;
size_t size = my_resources_file_config_json->size;
```

### With Runtime Library

```c
#include "my_resources.h"
#include <cirf/runtime.h>

// Find by path
const cirf_file_t *file = cirf_find_file(&my_resources_root, "config.json");

// Iterate all files
cirf_foreach_file_recursive(&my_resources_root, callback, NULL);

// Open as FILE*
FILE *fp = cirf_fopen(file);
```
