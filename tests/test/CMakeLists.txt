cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/* res/*)

add_executable(test WIN32)
target_link_libraries(test PRIVATE wcdx stdext Shlwapi Gdiplus)
target_include_directories(test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test PRIVATE _UNICODE UNICODE)
target_sources(test PRIVATE ${SOURCES})
target_version_info(test ${GENERATED_SOURCE_DIR}/res/version.rc "Test program for wcdx.dll")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)
