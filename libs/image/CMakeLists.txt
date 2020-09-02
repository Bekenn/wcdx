cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_library(image STATIC)
target_link_libraries(image PUBLIC stdext)
target_compile_definitions(image PRIVATE _UNICODE UNICODE)
target_include_directories(image PUBLIC include)

file(GLOB_RECURSE SOURCES include/* src/*)
file(GLOB_RECURSE RESOURCES res/*)
target_sources(image PRIVATE ${SOURCES} INTERFACE ${RESOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
