# ESP-IDF Project CMakeLists.txt
#
# This is the top-level CMakeLists.txt for the ESP32 CIRF example.
# It demonstrates embedding resources into an ESP32 application.

cmake_minimum_required(VERSION 3.16)

# Include the CIRF component from the parent project
# In a real project, you would copy or symlink the cirf directory
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../esp-idf"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cirf_esp32_example)
