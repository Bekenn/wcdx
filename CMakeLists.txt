cmake_minimum_required(VERSION 3.11 FATAL_ERROR)

set(CMAKE_USER_MAKE_RULES_OVERRIDE_C cmake/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX cmake/cxx_flag_overrides.cmake)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

project(wcdx)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/MP)
endif()

add_subdirectory(stdext)
add_subdirectory(wcdx)
add_subdirectory(test)
add_subdirectory(wcpatch)
add_subdirectory(patchmusic)
