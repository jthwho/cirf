# CIRF Component for ESP-IDF
#
# This component provides the cirf_runtime library (compiled for ESP32).
#
# For resource generation, include cirf_idf.cmake in your component:
#
#   # Get CIRF source directory from component
#   idf_component_get_property(CIRF_DIR cirf COMPONENT_DIR)
#   get_filename_component(CIRF_SOURCE_DIR "${CIRF_DIR}/../.." ABSOLUTE)
#   include(${CIRF_SOURCE_DIR}/esp-idf/cirf/cirf_idf.cmake)
#
#   cirf_generate(
#       NAME my_resources
#       CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../resources.json
#       OUTPUT_SOURCES MY_CIRF_SOURCES
#   )
#
#   idf_component_register(
#       SRCS "main.c" ${MY_CIRF_SOURCES}
#       INCLUDE_DIRS "." "${MY_CIRF_SOURCES_INCLUDE_DIR}"
#       REQUIRES cirf
#   )
#
# Configuration options (set in menuconfig or sdkconfig.defaults):
#   CONFIG_CIRF_MAX_PATH   - Maximum path length (default: 256)
#   CONFIG_CIRF_NO_STDIO   - Disable FILE* functions
#   CONFIG_CIRF_NO_MOUNT   - Disable mount system (recommended for ESP32)

# Get the CIRF source directory (two levels up from esp-idf/cirf)
get_filename_component(CIRF_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Register the runtime component
idf_component_register(
    SRCS "${CIRF_SOURCE_DIR}/src/runtime.c"
    INCLUDE_DIRS "${CIRF_SOURCE_DIR}/include"
)

# Apply configuration options as compile definitions
if(CONFIG_CIRF_MAX_PATH)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC CIRF_MAX_PATH=${CONFIG_CIRF_MAX_PATH})
endif()

if(CONFIG_CIRF_NO_STDIO)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC CIRF_NO_STDIO)
endif()

if(CONFIG_CIRF_NO_MOUNT)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC CIRF_NO_MOUNT)
endif()
