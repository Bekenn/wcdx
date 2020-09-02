cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_executable(wcres)
target_link_libraries(wcres PRIVATE stdext)
target_compile_definitions(wcres PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

file(GLOB_RECURSE SOURCES src/*)
target_sources(wcres PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
