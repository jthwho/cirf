cmake_minimum_required(VERSION 3.14)
project(simple_example C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =============================================================================
# CIRF Integration
# =============================================================================
#
# This example shows how to use CIRF in a standalone project.
#
# Option 1: Point to CIRF source directory (used here)
#   set(CIRF_SOURCE_DIR "/path/to/cirf")
#
# Option 2: Use installed CIRF
#   find_package(CIRF REQUIRED)
#
# Option 3: Add CIRF as a subdirectory
#   add_subdirectory(path/to/cirf)

# For this example, CIRF source is two levels up
get_filename_component(CIRF_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Include the CIRF CMake module
list(APPEND CMAKE_MODULE_PATH "${CIRF_SOURCE_DIR}/cmake")
include(CIRFGenerateResources)

# Ensure the cirf host tool is available (builds automatically if needed)
cirf_ensure_host_tool()

# Generate resources from configuration file
cirf_generate_resources(
    NAME simple_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
    OUTPUT_VAR RESOURCE_SOURCES
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/hello.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/data.json
)

# Add the CIRF runtime library (for cirf_find_file, cirf_fopen, etc.)
cirf_add_runtime_library()

# =============================================================================
# Application
# =============================================================================

add_executable(simple_example main.c ${RESOURCE_SOURCES})

target_include_directories(simple_example PRIVATE
    ${RESOURCE_SOURCES_INCLUDE_DIR}
)

target_link_libraries(simple_example PRIVATE cirf_runtime)
