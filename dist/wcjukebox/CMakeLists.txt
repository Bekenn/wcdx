cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

add_executable(wcjukebox)
target_link_libraries(wcjukebox PRIVATE stdext dsound)
target_compile_definitions(wcjukebox PRIVATE _UNICODE UNICODE _CRT_SECURE_NO_WARNINGS)
target_include_directories(wcjukebox PRIVATE src)

set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(GLOB_RECURSE SOURCES src/*)
unset(SOURCES_DISABLED)

file(GLOB_RECURSE SOURCES_WINDOWS src/windows/*)
if(NOT WIN32)
    list(APPEND SOURCES_DISABLED ${SOURCES_WINDOWS})
endif()

set_source_files_properties(${SOURCES_DISABLED} PROPERTIES LANGUAGE "")

target_sources(wcjukebox PRIVATE ${SOURCES})
target_version_info(wcjukebox ${GENERATED_SOURCE_DIR}/res/version.rc "Plays music")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${GENERATED_SOURCE_DIR} FILES ${GENERATED_SOURCE_DIR}/res/version.rc)
