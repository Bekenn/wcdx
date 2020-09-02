cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

add_subdirectory(wcdx)
add_subdirectory(wcpatch)
add_subdirectory(patchmusic)

set(CMAKE_FOLDER Tools)
add_subdirectory(wcimg)
add_subdirectory(wcjukebox)
add_subdirectory(wcres)
