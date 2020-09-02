cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

file(GLOB_RECURSE SOURCES src/*)

add_executable(patchmusic)
target_sources(patchmusic PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
