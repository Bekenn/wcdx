cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
include_guard(GLOBAL)

function(_VersionInfo_ParseVersion prefix version)
    if(NOT version MATCHES [[^([0-9]+)(\.([0-9]+))?(\.([0-9]+))?(\.([0-9]+))?]])
        message(FATAL_ERROR "Invalid version for ${prefix}: ${version}")
    endif()

    set(MAJOR ${CMAKE_MATCH_1})
    set(MINOR ${CMAKE_MATCH_3})
    set(PATCH ${CMAKE_MATCH_5})
    set(TWEAK ${CMAKE_MATCH_7})

    foreach(comp MINOR PATCH TWEAK)
        if(NOT ${comp})
            set(${comp} 0)
        endif()
    endforeach()

    set(${prefix}_MAJOR ${MAJOR} PARENT_SCOPE)
    set(${prefix}_MINOR ${MINOR} PARENT_SCOPE)
    set(${prefix}_PATCH ${PATCH} PARENT_SCOPE)
    set(${prefix}_TWEAK ${TWEAK} PARENT_SCOPE)
endfunction()

if(NOT CMAKE_CURRENT_LIST_FILE STREQUAL CMAKE_SCRIPT_MODE_FILE)
    # Running as a module; define the target_version_info function.
    function(target_version_info target output description)
        get_target_property(VERSION ${target} VERSION)
        if(NOT VERSION AND NOT PROJECT_VERSION)
            message(FATAL_ERROR "No version set for project ${PROJECT_NAME} or target ${target}")
        elseif(NOT VERSION)
            set(VERSION ${PROJECT_VERSION})
            set_target_properties(${target} PROPERTIES VERSION ${PROJECT_VERSION})
        elseif(NOT PROJECT_VERSION)
            set(PROJECT_VERSION ${VERSION})
        endif()

        # CMake is supposed to do this automatically, but doesn't:
        _VersionInfo_ParseVersion(VERSION ${VERSION})
        target_link_options(${target} PRIVATE /VERSION:${VERSION_MAJOR}.${VERSION_MINOR})

        get_filename_component(output ${output} ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
        add_custom_command(OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} --log-level=NOTICE -D OUTPUT=${output}
                -D NAME=${target} -D VERSION=${VERSION} -D FILENAME=$<TARGET_FILE_NAME:${target}> -D DESCRIPTION=${description}
                -D PROJECT_NAME=${PROJECT_NAME} -D PROJECT_VERSION=${PROJECT_VERSION} -D COMMENTS=${PROJECT_DESCRIPTION}
                -P ${CMAKE_CURRENT_FUNCTION_LIST_FILE}
            DEPENDS ${CMAKE_CURRENT_FUNCTION_LIST_DIR}/version.rc.in ${CMAKE_CURRENT_FUNCTION_LIST_FILE} ${CMAKE_BINARY_DIR}/CMakeCache.txt
            COMMENT "Generating ${output}..."
            VERBATIM
        )
        target_sources(${target} PRIVATE ${output})
    endfunction()

    # The rest of this file runs in script mode.
    return()
endif()

set(REQURIED_VARS OUTPUT NAME VERSION FILENAME DESCRIPTION PROJECT_NAME PROJECT_VERSION COMMENTS)
foreach(var ${REQUIRED_VARS})
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "Missing ${var}")
    endif()
endforeach()

_VersionInfo_ParseVersion(VERSION ${VERSION})
_VersionInfo_ParseVersion(PROJECT_VERSION ${PROJECT_VERSION})

find_package(Git)
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
        RESULT_VARIABLE GIT_RESULT
        OUTPUT_VARIABLE GIT_HASH
        ERROR_VARIABLE GIT_ERROR
    )
    if(GIT_RESULT EQUAL 0)
        string(STRIP ${GIT_HASH} GIT_HASH)
        set(GIT_HASH_ADDENDUM " (${GIT_HASH})")
    else()
        message(WARNING "git rev-parse failed: ${GIT_ERROR} (${GIT_RESULT})")
    endif()
endif()

string(TIMESTAMP COPYRIGHT_YEAR %Y)
configure_file(${CMAKE_CURRENT_LIST_DIR}/version.rc.in ${OUTPUT})
