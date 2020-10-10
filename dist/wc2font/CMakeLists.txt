cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

add_executable(wc2font)
target_link_libraries(wc2font PRIVATE image)
target_compile_definitions(wc2font PRIVATE _UNICODE UNICODE)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/*)
target_sources(wc2font PRIVATE ${SOURCES})
target_version_info(wc2font ${GENERATED_SOURCE_DIR}/res/version.rc "Extracts font data")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)

get_target_property(IMAGE_SOURCE_DIR image SOURCE_DIR)
get_target_property(IMAGE_INTERFACE_SOURCES image INTERFACE_SOURCES)
source_group(TREE ${IMAGE_SOURCE_DIR} FILES ${IMAGE_INTERFACE_SOURCES})
