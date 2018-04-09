cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

file(GLOB_RECURSE SOURCES src/* include/*)

add_library(iolib STATIC)
target_include_directories(iolib PUBLIC include)
target_compile_definitions(iolib PRIVATE _SCL_SECURE_NO_WARNINGS)
target_compile_features(iolib PUBLIC cxx_std_17)
target_sources(iolib PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
