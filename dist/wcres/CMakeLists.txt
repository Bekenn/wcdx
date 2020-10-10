cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

add_executable(wcres)
target_link_libraries(wcres PRIVATE stdext)
target_compile_definitions(wcres PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/*)
target_sources(wcres PRIVATE ${SOURCES})
target_version_info(wcres ${GENERATED_SOURCE_DIR}/res/version.rc "Extracts resource data")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)
