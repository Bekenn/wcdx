cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_C cmake/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX cmake/cxx_flag_overrides.cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

project(wcdx)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS UNICODE)

file(GLOB_RECURSE SOURCES src/* include/*)
set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(MIDL_GENERATED_SOURCES
    ${GENERATED_SOURCE_DIR}/include/iwcdx.h
    ${GENERATED_SOURCE_DIR}/src/iwcdx_i.c
    ${GENERATED_SOURCE_DIR}/src/iwcdx_p.c
    ${GENERATED_SOURCE_DIR}/src/dlldata.c
)

add_library(wcdx SHARED)
target_compile_definitions(wcdx PRIVATE WCDX_EXPORTS _SCL_SECURE_NO_WARNINGS)
target_include_directories(wcdx PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
target_link_libraries(wcdx PRIVATE d3d9 RpcRT4)
target_sources(wcdx PRIVATE ${SOURCES} ${MIDL_GENERATED_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_BINARY_DIR}/generated PREFIX "Source Files" FILES ${MIDL_GENERATED_SOURCES})

add_custom_command(OUTPUT ${MIDL_GENERATED_SOURCES}
    COMMAND midl /nologo /env win32 /out generated/src /h ../include/iwcdx.h /notlb /client none /server none
        ${CMAKE_CURRENT_SOURCE_DIR}/include/iwcdx.idl
    MAIN_DEPENDENCY include/iwcdx.idl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

add_subdirectory(test)
add_subdirectory(wcpatch)
add_subdirectory(patchmusic)
