# CIRFAddResources.cmake
# CMake function to generate resource files using CIRF

# cirf_add_resources(<name>
#     CONFIG <config_file>
#     [OUTPUT_DIR <output_directory>]
#     [LINK_RUNTIME]
#     [CIRF_EXECUTABLE <path>]
# )
#
# Creates a static library target <name> containing the embedded resources.
#
# Arguments:
#   <name>          - Name of the CMake target to create. Also used as the base
#                     name for generated C symbols.
#   CONFIG          - Path to the JSON configuration file
#   OUTPUT_DIR      - Directory for generated files (default: CMAKE_CURRENT_BINARY_DIR)
#   LINK_RUNTIME    - If specified, link against cirf_runtime library to get
#                     helper functions like cirf_find_file(), cirf_fopen(), etc.
#                     Without this, only direct symbol access is available.
#   CIRF_EXECUTABLE - Path to cirf executable (for cross-compilation)
#
# Cross-compilation:
#   When cross-compiling, the cirf executable must be built for the host machine.
#   Either:
#   1. Install cirf on the host and ensure it's in PATH
#   2. Set CIRF_EXECUTABLE to the path of the host-built cirf
#   3. Set the CIRF_HOST_EXECUTABLE cache variable before calling this function
#
# Example (without runtime - direct access only):
#   cirf_add_resources(game_assets
#       CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/assets.json
#   )
#   target_link_libraries(my_game PRIVATE game_assets)
#   # Access: game_assets_root, game_assets_file_icon_png, etc.
#
# Example (with runtime - helper functions available):
#   cirf_add_resources(game_assets
#       CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/assets.json
#       LINK_RUNTIME
#   )
#   target_link_libraries(my_game PRIVATE game_assets)
#   # Access: cirf_find_file(&game_assets_root, "path/to/file")
#
# Example (cross-compilation with explicit path):
#   cirf_add_resources(game_assets
#       CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/assets.json
#       CIRF_EXECUTABLE /usr/local/bin/cirf
#       LINK_RUNTIME
#   )

# Cache variable for cross-compilation - set before including this file
set(CIRF_HOST_EXECUTABLE "" CACHE FILEPATH "Path to host-built cirf executable (for cross-compilation)")

function(cirf_add_resources name)
    cmake_parse_arguments(ARG "LINK_RUNTIME" "CONFIG;OUTPUT_DIR;CIRF_EXECUTABLE" "" ${ARGN})

    if(NOT ARG_CONFIG)
        message(FATAL_ERROR "cirf_add_resources: CONFIG is required")
    endif()

    if(NOT ARG_OUTPUT_DIR)
        set(ARG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # Ensure output directory exists
    file(MAKE_DIRECTORY ${ARG_OUTPUT_DIR})

    set(OUTPUT_C ${ARG_OUTPUT_DIR}/${name}.c)
    set(OUTPUT_H ${ARG_OUTPUT_DIR}/${name}.h)

    # Find the cirf executable - priority order:
    # 1. Explicit CIRF_EXECUTABLE argument
    # 2. CIRF_HOST_EXECUTABLE cache variable (for cross-compilation)
    # 3. cirf target (when building as part of CIRF)
    # 4. cirf in PATH
    if(ARG_CIRF_EXECUTABLE)
        set(CIRF_EXECUTABLE ${ARG_CIRF_EXECUTABLE})
        set(CIRF_DEPENDS "")
    elseif(CIRF_HOST_EXECUTABLE)
        set(CIRF_EXECUTABLE ${CIRF_HOST_EXECUTABLE})
        set(CIRF_DEPENDS "")
    elseif(TARGET cirf)
        set(CIRF_EXECUTABLE $<TARGET_FILE:cirf>)
        set(CIRF_DEPENDS cirf)
    else()
        find_program(CIRF_EXECUTABLE_FOUND cirf)
        if(NOT CIRF_EXECUTABLE_FOUND)
            if(CMAKE_CROSSCOMPILING)
                message(FATAL_ERROR
                    "cirf_add_resources: cirf executable not found.\n"
                    "When cross-compiling, you must either:\n"
                    "  1. Install cirf on the host system (in PATH)\n"
                    "  2. Set CIRF_HOST_EXECUTABLE to the path of host-built cirf\n"
                    "  3. Pass CIRF_EXECUTABLE argument to cirf_add_resources()")
            else()
                message(FATAL_ERROR "cirf_add_resources: cirf executable not found")
            endif()
        endif()
        set(CIRF_EXECUTABLE ${CIRF_EXECUTABLE_FOUND})
        set(CIRF_DEPENDS "")
    endif()

    # Get absolute path to config file
    get_filename_component(CONFIG_ABS ${ARG_CONFIG} ABSOLUTE)

    # Collect all source files from config for dependency tracking
    # (This is a simplified version - ideally we'd parse the JSON)
    set(CONFIG_DEPS ${CONFIG_ABS})

    add_custom_command(
        OUTPUT ${OUTPUT_C} ${OUTPUT_H}
        COMMAND ${CIRF_EXECUTABLE}
            -n ${name}
            -c ${CONFIG_ABS}
            -o ${OUTPUT_C}
            -H ${OUTPUT_H}
        DEPENDS ${CIRF_DEPENDS} ${CONFIG_DEPS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating ${name} resources"
        VERBATIM
    )

    add_library(${name} STATIC ${OUTPUT_C})
    target_include_directories(${name} PUBLIC ${ARG_OUTPUT_DIR})

    # Need CIRF include directory for <cirf/types.h>
    # Priority: cirf_runtime target > cirf_lib target > installed path
    if(TARGET cirf_runtime)
        # Get include dirs from cirf_runtime (preferred for cross-compilation)
        get_target_property(CIRF_INCLUDE_DIRS cirf_runtime INTERFACE_INCLUDE_DIRECTORIES)
        if(CIRF_INCLUDE_DIRS)
            target_include_directories(${name} PUBLIC ${CIRF_INCLUDE_DIRS})
        endif()
    elseif(TARGET cirf_lib)
        # Building as part of CIRF with generator - get include dirs from cirf_lib
        get_target_property(CIRF_INCLUDE_DIRS cirf_lib INTERFACE_INCLUDE_DIRECTORIES)
        if(CIRF_INCLUDE_DIRS)
            target_include_directories(${name} PUBLIC ${CIRF_INCLUDE_DIRS})
        endif()
    else()
        # Using installed CIRF - find include directory
        find_path(CIRF_INCLUDE_DIR cirf/types.h)
        if(CIRF_INCLUDE_DIR)
            target_include_directories(${name} PUBLIC ${CIRF_INCLUDE_DIR})
        else()
            message(WARNING "cirf_add_resources: cirf/types.h not found in include path")
        endif()
    endif()

    # Link against cirf_runtime if requested
    if(ARG_LINK_RUNTIME)
        if(TARGET cirf_runtime)
            target_link_libraries(${name} PUBLIC cirf_runtime)
        else()
            # Try to find installed cirf_runtime
            find_library(CIRF_RUNTIME_LIB cirf_runtime)
            if(CIRF_RUNTIME_LIB)
                target_link_libraries(${name} PUBLIC ${CIRF_RUNTIME_LIB})
            else()
                message(WARNING "cirf_add_resources: LINK_RUNTIME requested but cirf_runtime not found")
            endif()
        endif()
    endif()

    # Mark generated files as generated
    set_source_files_properties(${OUTPUT_C} ${OUTPUT_H} PROPERTIES GENERATED TRUE)
endfunction()
