cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

file(GLOB_RECURSE SOURCES src/* res/*)

add_executable(test WIN32)
target_link_libraries(test PRIVATE wcdx stdext Shlwapi Gdiplus)
target_include_directories(test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(test PRIVATE _UNICODE)
target_sources(test PRIVATE ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
