cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

add_executable(wcjukebox)
target_link_libraries(wcjukebox PRIVATE stdext dsound)
target_compile_definitions(wcjukebox PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

file(GLOB_RECURSE SOURCES src/*)
target_sources(wcjukebox PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
