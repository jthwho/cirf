# CIRF.cmake
#
# CMake module for generating CIRF resources with cross-compilation support.
# This module handles building the cirf tool for the host and generating
# resources that can be compiled for any target platform.
#
# Features:
#   - Automatic cirf tool building from source when needed
#   - Build-time dependency tracking via DEPFILE (modifying source files triggers regeneration)
#   - Cross-compilation support for embedded targets
#
# Functions:
#   cirf_generate_resources()   - Generate resources, return source files in variable
#   cirf_add_runtime_library()  - Add the CIRF runtime library for helper functions
#
# Usage:
#   include(CIRF)
#   cirf_generate_resources(
#       NAME my_resources
#       CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/resources.json
#       OUTPUT_VAR MY_RESOURCE_SOURCES
#   )
#   add_executable(my_app main.c ${MY_RESOURCE_SOURCES})

include(ExternalProject)

# Cache variable for pre-built executable (optional)
set(CIRF_HOST_EXECUTABLE "" CACHE FILEPATH "Path to pre-built host cirf executable")

# Internal: Find CIRF source directory
# Checks in order:
#   1. CIRF_SOURCE_DIR variable (set by user before including this module)
#   2. Relative to this cmake file (when used from CIRF source tree)
function(_cirf_find_source_dir out_var)
    # Check if CIRF_SOURCE_DIR was set by the including project
    if(DEFINED CIRF_SOURCE_DIR AND NOT "${CIRF_SOURCE_DIR}" STREQUAL "")
        set(${out_var} "${CIRF_SOURCE_DIR}" PARENT_SCOPE)
        return()
    endif()

    # Check common locations relative to this file
    get_filename_component(_this_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
    get_filename_component(_cirf_root "${_this_dir}/.." ABSOLUTE)

    if(EXISTS "${_cirf_root}/src/main.c" AND EXISTS "${_cirf_root}/include/cirf/types.h")
        set(${out_var} "${_cirf_root}" PARENT_SCOPE)
        return()
    endif()

    # Not found
    set(${out_var} "" PARENT_SCOPE)
endfunction()

# Internal: Ensure the cirf executable is available for the host machine.
# In cross-compilation scenarios, this builds cirf using the host compiler.
# Sets CIRF_EXECUTABLE in parent scope.
function(_cirf_ensure_host_tool)
    # Already have an executable?
    if(CIRF_HOST_EXECUTABLE AND EXISTS "${CIRF_HOST_EXECUTABLE}")
        set(CIRF_EXECUTABLE "${CIRF_HOST_EXECUTABLE}" PARENT_SCOPE)
        message(STATUS "CIRF: Using pre-built host tool: ${CIRF_HOST_EXECUTABLE}")
        return()
    endif()

    # Check if cirf target exists (building as part of CIRF, not cross-compiling)
    if(TARGET cirf AND NOT CMAKE_CROSSCOMPILING)
        set(CIRF_EXECUTABLE "$<TARGET_FILE:cirf>" PARENT_SCOPE)
        message(STATUS "CIRF: Using cirf target from current build")
        return()
    endif()

    # Try to find cirf in PATH
    find_program(_cirf_in_path cirf)
    if(_cirf_in_path)
        set(CIRF_EXECUTABLE "${_cirf_in_path}" PARENT_SCOPE)
        message(STATUS "CIRF: Found cirf in PATH: ${_cirf_in_path}")
        return()
    endif()

    # Need to build cirf for host - find source directory
    _cirf_find_source_dir(_cirf_src)
    if(NOT _cirf_src)
        message(FATAL_ERROR
            "CIRF: Cannot find CIRF source directory.\n"
            "Set CIRF_SOURCE_DIR to the CIRF repository root, or\n"
            "Set CIRF_HOST_EXECUTABLE to a pre-built cirf binary.")
    endif()

    message(STATUS "CIRF: Building host tool from ${_cirf_src}")

    # Build directory for host tool
    set(_host_build_dir "${CMAKE_BINARY_DIR}/cirf-host-build")
    set(_host_cirf "${_host_build_dir}/cirf")

    # Determine the compiler for the host tool
    set(_cirf_cc "${CMAKE_C_COMPILER}")
    if(CMAKE_CROSSCOMPILING)
        find_program(_host_cc NAMES cc gcc clang)
        if(_host_cc)
            set(_cirf_cc "${_host_cc}")
        endif()
    endif()

    # Use ExternalProject to build cirf with host compiler
    ExternalProject_Add(cirf_host_tool
        SOURCE_DIR "${_cirf_src}"
        BINARY_DIR "${_host_build_dir}"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCIRF_BUILD_RUNTIME=OFF
            -DCIRF_BUILD_EXAMPLES=OFF
            -DCMAKE_C_COMPILER=${_cirf_cc}
            -DCMAKE_TOOLCHAIN_FILE=
        BUILD_COMMAND ${CMAKE_COMMAND} --build . --target cirf
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS "${_host_cirf}"
        EXCLUDE_FROM_ALL TRUE
    )

    set(CIRF_EXECUTABLE "${_host_cirf}" PARENT_SCOPE)
    set(CIRF_HOST_TOOL_TARGET "cirf_host_tool" PARENT_SCOPE)
endfunction()

#
# cirf_generate_resources(
#     NAME <name>
#     CONFIG <config_file>
#     OUTPUT_VAR <variable_name>
#     [DEPENDS <file1> <file2> ...]
# )
#
# Generates CIRF resources and sets the output variable to the list of
# generated source files that should be compiled into the target.
#
# Arguments:
#   NAME       - Base name for generated symbols (e.g., "web_resources")
#   CONFIG     - Path to the JSON configuration file
#   OUTPUT_VAR - Name of variable to set with generated source file paths
#   DEPENDS    - Additional files that trigger regeneration (optional)
#
# The generated files are placed in CMAKE_CURRENT_BINARY_DIR.
#
function(cirf_generate_resources)
    cmake_parse_arguments(ARG "" "NAME;CONFIG;OUTPUT_VAR" "DEPENDS" ${ARGN})

    if(NOT ARG_NAME)
        message(FATAL_ERROR "cirf_generate_resources: NAME is required")
    endif()
    if(NOT ARG_CONFIG)
        message(FATAL_ERROR "cirf_generate_resources: CONFIG is required")
    endif()
    if(NOT ARG_OUTPUT_VAR)
        message(FATAL_ERROR "cirf_generate_resources: OUTPUT_VAR is required")
    endif()

    # Ensure we have the host tool (builds automatically if needed)
    if(NOT CIRF_EXECUTABLE)
        _cirf_ensure_host_tool()
    endif()

    # Get absolute path to config
    get_filename_component(_config_abs "${ARG_CONFIG}" ABSOLUTE)
    get_filename_component(_config_dir "${_config_abs}" DIRECTORY)

    # Output paths
    set(_out_c "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.c")
    set(_out_h "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.h")
    set(_out_d "${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}.d")

    # Build dependency list - start with config file
    set(_depends "${_config_abs}")

    # Add manually specified dependencies
    if(ARG_DEPENDS)
        list(APPEND _depends ${ARG_DEPENDS})
    endif()

    # Add host tool target as dependency if building it
    if(CIRF_HOST_TOOL_TARGET)
        list(APPEND _depends ${CIRF_HOST_TOOL_TARGET})
    endif()

    # Custom command to generate resources
    # Uses DEPFILE so that source file dependencies are tracked at build time
    add_custom_command(
        OUTPUT "${_out_c}" "${_out_h}"
        COMMAND "${CIRF_EXECUTABLE}"
            -n "${ARG_NAME}"
            -c "${_config_abs}"
            -o "${_out_c}"
            -H "${_out_h}"
            -M "${_out_d}"
        DEPENDS ${_depends}
        DEPFILE "${_out_d}"
        WORKING_DIRECTORY "${_config_dir}"
        COMMENT "CIRF: Generating ${ARG_NAME} resources"
        VERBATIM
    )

    # Create a target for the generation
    add_custom_target(generate_${ARG_NAME}
        DEPENDS "${_out_c}" "${_out_h}"
    )

    # Set output variable with generated source file
    set(${ARG_OUTPUT_VAR} "${_out_c}" PARENT_SCOPE)

    # Also set header path in case caller needs it
    set(${ARG_OUTPUT_VAR}_HEADER "${_out_h}" PARENT_SCOPE)
    set(${ARG_OUTPUT_VAR}_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)
endfunction()

#
# cirf_add_runtime_library([target_name])
#
# Adds the CIRF runtime library as a target. For cross-compilation,
# this builds the runtime with the target compiler.
#
# Arguments:
#   target_name - Optional name for the library target (default: cirf_runtime)
#
function(cirf_add_runtime_library)
    set(_target_name "cirf_runtime")
    if(ARGC GREATER 0)
        set(_target_name "${ARGV0}")
    endif()

    # Find CIRF source directory
    _cirf_find_source_dir(_cirf_src)
    if(NOT _cirf_src)
        message(FATAL_ERROR
            "CIRF: Cannot find CIRF source directory for runtime library.\n"
            "Set CIRF_SOURCE_DIR to the CIRF repository root.")
    endif()

    # Create runtime library target
    add_library(${_target_name} STATIC
        "${_cirf_src}/src/runtime.c"
    )

    target_include_directories(${_target_name} PUBLIC
        "${_cirf_src}/include"
    )

    # Apply configuration options if set
    if(CIRF_RUNTIME_NO_STDIO)
        target_compile_definitions(${_target_name} PUBLIC CIRF_NO_STDIO)
    endif()
    if(CIRF_RUNTIME_NO_MOUNT)
        target_compile_definitions(${_target_name} PUBLIC CIRF_NO_MOUNT)
    endif()
    if(CIRF_RUNTIME_MAX_PATH)
        target_compile_definitions(${_target_name} PUBLIC CIRF_MAX_PATH=${CIRF_RUNTIME_MAX_PATH})
    endif()
endfunction()
