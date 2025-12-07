# CIRF Component for ESP-IDF
#
# This component provides:
#   1. The cirf_runtime library (compiled for ESP32)
#   2. The cirf_generate() function for generating resources at build time
#   3. Automatic building of the cirf host tool when needed
#
# Usage:
#   In your main component's CMakeLists.txt:
#
#     cirf_generate(
#         NAME my_resources
#         CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../resources.json
#         DEPENDS
#             ${CMAKE_CURRENT_SOURCE_DIR}/files/index.html
#             ${CMAKE_CURRENT_SOURCE_DIR}/files/style.css
#         OUTPUT_SOURCES MY_CIRF_SOURCES
#     )
#
#     idf_component_register(
#         SRCS "main.c" ${MY_CIRF_SOURCES}
#         INCLUDE_DIRS "." "${MY_CIRF_SOURCES_INCLUDE_DIR}"
#         REQUIRES cirf
#     )
#
# Configuration options (set in menuconfig or sdkconfig.defaults):
#   CONFIG_CIRF_MAX_PATH   - Maximum path length (default: 256)
#   CONFIG_CIRF_NO_STDIO   - Disable FILE* functions
#   CONFIG_CIRF_NO_MOUNT   - Disable mount system (recommended for ESP32)

# Get the CIRF source directory (parent of idf_component)
get_filename_component(CIRF_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

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

# ============================================================================
# Host tool management
# ============================================================================

# Directory for building the host tool
set(CIRF_HOST_BUILD_DIR "${CMAKE_BINARY_DIR}/cirf-host" CACHE INTERNAL "")
set(CIRF_HOST_EXECUTABLE "${CIRF_HOST_BUILD_DIR}/cirf" CACHE INTERNAL "")

# Function to ensure the host tool is built
function(cirf_ensure_host_tool)
    if(TARGET cirf_host_build)
        return()  # Already set up
    endif()

    # Check if cirf is in PATH
    find_program(_cirf_in_path cirf)
    if(_cirf_in_path)
        set(CIRF_HOST_EXECUTABLE "${_cirf_in_path}" CACHE INTERNAL "" FORCE)
        message(STATUS "CIRF: Using cirf from PATH: ${_cirf_in_path}")
        # Create dummy target
        add_custom_target(cirf_host_build)
        return()
    endif()

    # Check if user specified a path
    if(DEFINED CACHE{CIRF_EXECUTABLE} AND EXISTS "${CIRF_EXECUTABLE}")
        set(CIRF_HOST_EXECUTABLE "${CIRF_EXECUTABLE}" CACHE INTERNAL "" FORCE)
        message(STATUS "CIRF: Using specified cirf: ${CIRF_EXECUTABLE}")
        add_custom_target(cirf_host_build)
        return()
    endif()

    message(STATUS "CIRF: Building host tool from ${CIRF_SOURCE_DIR}")

    # Find host C compiler
    find_program(_host_cc NAMES gcc cc clang
        PATHS /usr/bin /usr/local/bin
        NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
    )
    if(NOT _host_cc)
        message(FATAL_ERROR
            "CIRF: Cannot find host C compiler to build cirf.\n"
            "Either install cirf system-wide, or set CIRF_EXECUTABLE.")
    endif()

    # Create build directory
    file(MAKE_DIRECTORY "${CIRF_HOST_BUILD_DIR}")

    # Configure and build cirf for host at configure time
    # (This ensures it's ready before any targets need it)
    message(STATUS "CIRF: Configuring host build with ${_host_cc}")

    execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DCMAKE_C_COMPILER=${_host_cc}
            -DCMAKE_BUILD_TYPE=Release
            -DCIRF_BUILD_RUNTIME=OFF
            -DCIRF_BUILD_EXAMPLES=OFF
            -DCIRF_BUILD_GENERATOR=ON
            ${CIRF_SOURCE_DIR}
        WORKING_DIRECTORY "${CIRF_HOST_BUILD_DIR}"
        RESULT_VARIABLE _config_result
        OUTPUT_VARIABLE _config_output
        ERROR_VARIABLE _config_error
    )

    if(_config_result)
        message(FATAL_ERROR "CIRF: Failed to configure host build:\n${_config_error}")
    endif()

    # Build target that builds the host tool
    add_custom_command(
        OUTPUT "${CIRF_HOST_EXECUTABLE}"
        COMMAND ${CMAKE_COMMAND} --build . --target cirf
        WORKING_DIRECTORY "${CIRF_HOST_BUILD_DIR}"
        COMMENT "CIRF: Building host tool"
        VERBATIM
    )

    add_custom_target(cirf_host_build
        DEPENDS "${CIRF_HOST_EXECUTABLE}"
    )

    message(STATUS "CIRF: Host tool will be built at: ${CIRF_HOST_EXECUTABLE}")
endfunction()

# ============================================================================
# Resource generation function
# ============================================================================

#
# cirf_generate(
#     NAME <name>
#     CONFIG <config_file>
#     OUTPUT_SOURCES <variable_name>
#     [DEPENDS <file1> <file2> ...]
#     [WORKING_DIRECTORY <dir>]
# )
#
# Generates CIRF resources at build time.
#
# Arguments:
#   NAME             - Base name for generated symbols (e.g., "web_resources")
#   CONFIG           - Path to the JSON configuration file
#   OUTPUT_SOURCES   - Variable to set with path to generated .c file
#   DEPENDS          - Additional files that trigger regeneration
#   WORKING_DIRECTORY - Working directory for cirf (default: CONFIG file's directory)
#
# Also sets:
#   <OUTPUT_SOURCES>_INCLUDE_DIR - Directory containing the generated header
#
function(cirf_generate)
    cmake_parse_arguments(ARG "" "NAME;CONFIG;OUTPUT_SOURCES;WORKING_DIRECTORY" "DEPENDS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "cirf_generate: NAME is required")
    endif()
    if(NOT ARG_CONFIG)
        message(FATAL_ERROR "cirf_generate: CONFIG is required")
    endif()
    if(NOT ARG_OUTPUT_SOURCES)
        message(FATAL_ERROR "cirf_generate: OUTPUT_SOURCES is required")
    endif()

    # Ensure host tool is available
    cirf_ensure_host_tool()

    # Get absolute paths
    get_filename_component(_config_abs "${ARG_CONFIG}" ABSOLUTE)

    if(ARG_WORKING_DIRECTORY)
        set(_work_dir "${ARG_WORKING_DIRECTORY}")
    else()
        get_filename_component(_work_dir "${_config_abs}" DIRECTORY)
    endif()

    # Output paths in build directory
    set(_out_c "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.c")
    set(_out_h "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.h")

    # Custom command to generate resources
    add_custom_command(
        OUTPUT "${_out_c}" "${_out_h}"
        COMMAND "${CIRF_HOST_EXECUTABLE}"
            -n "${ARG_NAME}"
            -c "${_config_abs}"
            -o "${_out_c}"
            -H "${_out_h}"
        DEPENDS
            cirf_host_build
            "${_config_abs}"
            ${ARG_DEPENDS}
        WORKING_DIRECTORY "${_work_dir}"
        COMMENT "CIRF: Generating ${ARG_NAME} from ${ARG_CONFIG}"
        VERBATIM
    )

    # Set output variables in parent scope
    set(${ARG_OUTPUT_SOURCES} "${_out_c}" PARENT_SCOPE)
    set(${ARG_OUTPUT_SOURCES}_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
endfunction()
