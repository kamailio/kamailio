cmake_minimum_required(VERSION 3.10)

# -----------------------
#  Main project name
# -----------------------
# main binary name
set(MAIN_NAME "kamailio")
# use kamailio config
set(CFG_NAME "kamailio")

include(defs.cmake)


# -----------------------
# The following produces core/autover.h
# Find Git
find_package(Git QUIET)

if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --verify --short=6 HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE REPO_VER
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    execute_process(
        COMMAND bash -c "${GIT_EXECUTABLE} diff-index --name-only HEAD | grep -vE 'Makefile|CMakeLists.txt'"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DIFF_OUTPUT
    )
    set(REPO_HASH ${REPO_VER})

    if(NOT "${GIT_DIFF_OUTPUT}" STREQUAL "")
        set(REPO_VER "${REPO_VER}-dirty")
    endif()

    string(REGEX REPLACE "(.*)-dirty" "dirty" REPO_STATE ${REPO_VER})
else()
    set(REPO_VER "")
    set(REPO_HASH "unknown")
    set(REPO_STATE "")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/core/autover.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/core/autover.h"
)
#  -----------------------
add_subdirectory(core)