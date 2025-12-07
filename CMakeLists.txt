cmake_minimum_required(VERSION 3.14)

project(cirf
    VERSION 0.1.0
    DESCRIPTION "C Inline Resource Filesystem - compile files into C code"
    LANGUAGES C
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(CIRF_BUILD_EXAMPLES "Build example projects" OFF)
option(CIRF_BUILD_RUNTIME "Build the optional runtime library" ON)
option(CIRF_BUILD_GENERATOR "Build the code generator (disable for cross-compilation)" ON)

# Runtime library configuration for embedded targets
option(CIRF_RUNTIME_NO_STDIO "Disable FILE* functions in runtime" OFF)
option(CIRF_RUNTIME_NO_MOUNT "Disable mount system in runtime (avoids malloc)" OFF)
set(CIRF_RUNTIME_MAX_PATH "" CACHE STRING "Maximum path length for runtime (empty = default 256)")

# Source files for the code generator
set(CIRF_SOURCES
    src/error.c
    src/json.c
    src/mime.c
    src/vfs.c
    src/glob.c
    src/config.c
    src/codegen.c
    src/writer.c
)

# Code generator - only build when not cross-compiling (or when explicitly requested)
if(CIRF_BUILD_GENERATOR AND NOT CMAKE_CROSSCOMPILING)
    # Library for the code generator (for potential embedding)
    add_library(cirf_lib STATIC ${CIRF_SOURCES})
    target_include_directories(cirf_lib PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(cirf_lib PROPERTIES OUTPUT_NAME cirf)

    # Code generator executable
    add_executable(cirf src/main.c)
    target_link_libraries(cirf PRIVATE cirf_lib)
    target_include_directories(cirf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cirf_lib PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(cirf PRIVATE -Wall -Wextra -Wpedantic)
    endif()
elseif(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling: skipping cirf code generator (use host-built cirf)")
endif()

# Runtime library (optional, for helper functions like cirf_find_file)
# This library is linked by applications that want to use the runtime API
# with their generated resources. Applications that only use direct symbol
# access do not need this library.
#
# For cross-compilation, this is the main target to build.
if(CIRF_BUILD_RUNTIME)
    add_library(cirf_runtime STATIC src/runtime.c)
    target_include_directories(cirf_runtime PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    set_target_properties(cirf_runtime PROPERTIES OUTPUT_NAME cirf_runtime)

    # Apply runtime configuration
    if(CIRF_RUNTIME_NO_STDIO)
        target_compile_definitions(cirf_runtime PUBLIC CIRF_NO_STDIO)
    endif()
    if(CIRF_RUNTIME_NO_MOUNT)
        target_compile_definitions(cirf_runtime PUBLIC CIRF_NO_MOUNT)
    endif()
    if(CIRF_RUNTIME_MAX_PATH)
        target_compile_definitions(cirf_runtime PUBLIC CIRF_MAX_PATH=${CIRF_RUNTIME_MAX_PATH})
    endif()

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(cirf_runtime PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

# Install targets
include(GNUInstallDirs)

set(CIRF_INSTALL_TARGETS "")
if(TARGET cirf)
    list(APPEND CIRF_INSTALL_TARGETS cirf)
endif()
if(TARGET cirf_lib)
    list(APPEND CIRF_INSTALL_TARGETS cirf_lib)
endif()
if(TARGET cirf_runtime)
    list(APPEND CIRF_INSTALL_TARGETS cirf_runtime)
endif()

if(CIRF_INSTALL_TARGETS)
    install(TARGETS ${CIRF_INSTALL_TARGETS}
        EXPORT cirfTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(DIRECTORY include/cirf
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(FILES cmake/CIRFConfig.cmake cmake/CIRFAddResources.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CIRF
)

install(EXPORT cirfTargets
    FILE CIRFTargets.cmake
    NAMESPACE CIRF::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CIRF
)

# Make cirf_add_resources available when used as subdirectory
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CIRFAddResources.cmake)

# Examples
if(CIRF_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
