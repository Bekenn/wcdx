cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/* res/*)

add_executable(wcpatch)
target_link_libraries(wcpatch PRIVATE stdext)
target_include_directories(wcpatch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} src)
target_compile_definitions(wcpatch PRIVATE _UNICODE UNICODE _SCL_SECURE_NO_WARNINGS)
target_sources(wcpatch PRIVATE ${SOURCES})
target_version_info(wcpatch ${GENERATED_SOURCE_DIR}/res/version.rc "Patches game executables")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)
