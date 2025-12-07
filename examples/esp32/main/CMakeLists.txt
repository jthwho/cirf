# Main component CMakeLists.txt
#
# This component contains the application code and generated resources.
# Resources are generated automatically at build time using cirf_generate().

# Generate resources - cirf host tool is built automatically if needed
cirf_generate(
    NAME web_resources
    CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/../resources.json
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/css/style.css
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/api/config.json
    OUTPUT_SOURCES WEB_RESOURCES_SOURCES
)

idf_component_register(
    SRCS
        "main.c"
        ${WEB_RESOURCES_SOURCES}
    INCLUDE_DIRS
        "."
        "${WEB_RESOURCES_SOURCES_INCLUDE_DIR}"
    REQUIRES
        cirf
)
