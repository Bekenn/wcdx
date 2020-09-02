cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_executable(wc2font)
target_link_libraries(wc2font PRIVATE image)
target_compile_definitions(wc2font PRIVATE _UNICODE UNICODE)

file(GLOB_RECURSE SOURCES src/*)
target_sources(wc2font PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

get_target_property(IMAGE_SOURCE_DIR image SOURCE_DIR)
get_target_property(IMAGE_INTERFACE_SOURCES image INTERFACE_SOURCES)
source_group(TREE ${IMAGE_SOURCE_DIR} FILES ${IMAGE_INTERFACE_SOURCES})
