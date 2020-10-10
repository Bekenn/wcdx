cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/*)

add_executable(patchmusic)
target_version_info(patchmusic ${GENERATED_SOURCE_DIR}/res/version.rc "Patches music streams")
target_sources(patchmusic PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)
