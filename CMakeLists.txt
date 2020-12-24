cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_VS_GLOBALS XPDeprecationWarning=false)

project(wcdx
    VERSION 2.2.0.0
    DESCRIPTION "Wing Commander patches for modern Windows operating systems"
    HOMEPAGE_URL https://github.com/Bekenn/wcdx/
)

if(NOT CMAKE_VS_PLATFORM_TOOLSET)
    message(FATAL_ERROR "wcdx must be built using Visual Studio")
endif()
if(NOT CMAKE_VS_PLATFORM_TOOLSET MATCHES [[_xp(,.*)?$]])
    message(FATAL_ERROR "Please select a platform toolset supporting Windows XP (e.g. -T v141_xp)")
endif()
if(NOT CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
    message(FATAL_ERROR "wcdx must be built for 32-bit x86 (-A Win32)")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
if(MSVC)
    add_compile_options(/MP /WX /W4 /wd4100)   # C4100: unreferenced formal parameter
endif()
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=_WIN32_WINNT_WINXP)
endif()

add_subdirectory(external)
add_subdirectory(libs)
add_subdirectory(dist)
add_subdirectory(tests)
