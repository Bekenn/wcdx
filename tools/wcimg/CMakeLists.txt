cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

add_executable(wcimg)
target_link_libraries(wcimg PRIVATE stdext)
target_compile_definitions(wcimg PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

file(GLOB_RECURSE SOURCES src/* res/*)
target_sources(wcimg PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
