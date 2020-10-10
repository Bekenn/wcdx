cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

add_executable(wcimg)
target_link_libraries(wcimg PRIVATE image)
target_compile_definitions(wcimg PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/*)
target_sources(wcimg PRIVATE ${SOURCES})
target_version_info(wcimg ${GENERATED_SOURCE_DIR}/res/version.rc "Extracts image data")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)

get_target_property(IMAGE_SOURCE_DIR image SOURCE_DIR)
get_target_property(IMAGE_INTERFACE_SOURCES image INTERFACE_SOURCES)
source_group(TREE ${IMAGE_SOURCE_DIR} FILES ${IMAGE_INTERFACE_SOURCES})
