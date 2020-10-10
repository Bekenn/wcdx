cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

include(VersionInfo)

file(GLOB_RECURSE SOURCES src/* idl/* res/*)
set(GENERATED_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(MIDL_GENERATED_SOURCES
    ${GENERATED_SOURCE_DIR}/include/iwcdx.h
    ${GENERATED_SOURCE_DIR}/src/iwcdx_i.c
    ${GENERATED_SOURCE_DIR}/src/iwcdx_p.c
    ${GENERATED_SOURCE_DIR}/src/dlldata.c
)
set(GENERATED_VERSION_RC ${GENERATED_SOURCE_DIR}/res/version.rc)
set(GENERATED_SOURCES ${MIDL_GENERATED_SOURCES} ${GENERATED_VERSION_RC})

add_library(wcdx SHARED)
set_target_properties(wcdx PROPERTIES WIN32_EXECUTABLE true)
target_compile_definitions(wcdx PRIVATE WCDX_EXPORTS _UNICODE UNICODE _SCL_SECURE_NO_WARNINGS)
target_include_directories(wcdx PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
target_link_libraries(wcdx PRIVATE stdext d3d9 RpcRT4)
target_version_info(wcdx ${GENERATED_VERSION_RC} "wcdx support library")
target_sources(wcdx PRIVATE ${SOURCES} ${MIDL_GENERATED_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_BINARY_DIR}/generated FILES ${GENERATED_SOURCES})

add_custom_command(OUTPUT ${MIDL_GENERATED_SOURCES}
    COMMAND midl /nologo /env win32 /char ascii7 /out generated/src /h ../include/iwcdx.h /notlb /client none /server none
        ${CMAKE_CURRENT_SOURCE_DIR}/idl/iwcdx.idl
    MAIN_DEPENDENCY idl/iwcdx.idl
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)
