cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

set(CMAKE_GENERATOR_PLATFORM Win32)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

project(wcdx)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/MP /permissive- /WX /W4 /wd4100)   # C4100: unreferenced formal parameter
endif()

add_subdirectory(external)
add_subdirectory(libs)
add_subdirectory(dist)
add_subdirectory(tests)
