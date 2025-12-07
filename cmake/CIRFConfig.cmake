# CIRFConfig.cmake
# Config file for the CIRF package

include(CMakeFindDependencyMacro)

# Include targets if available
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/CIRFTargets.cmake")
    include("${CMAKE_CURRENT_LIST_DIR}/CIRFTargets.cmake")
endif()

# Include the add_resources function
include("${CMAKE_CURRENT_LIST_DIR}/CIRFAddResources.cmake")

# Provide version info
set(CIRF_VERSION 0.1.0)
