cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

file(GLOB_RECURSE SOURCES src/* res/*)

add_executable(wcpatch)
target_link_libraries(wcpatch PRIVATE iolib)
target_compile_definitions(wcpatch PRIVATE _UNICODE _SCL_SECURE_NO_WARNINGS)
target_sources(wcpatch PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
