cmake_minimum_required(VERSION 3.10)



include(GNUInstallDirs)

# # Check if LIBDIR is not defined
# if(NOT DEFINED LIBDIR)
#     # Get the system architecture
#     if(CMAKE_SIZEOF_VOID_P EQUAL 8)
#         set(LIBDIR "lib64")
#     else()
#         set(LIBDIR "lib")
#     endif()
# endif()

# Set the installation paths
message(STATUS "Cross compile: ${CMAKE_CROSSCOMPILING}")

message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
message(STATUS "CMAKE_INSTALL_BINDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}")
message(STATUS "CMAKE_INSTALL_SBINDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SBINDIR}")
message(STATUS "CMAKE_INSTALL_SYSCONFDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}")
message(STATUS "CMAKE_INSTALL_LOCALSTATEDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/run/${MAIN_NAME}")
message(STATUS "CMAKE_INSTALL_DATADIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}")
message(STATUS "CMAKE_INSTALL_DOCDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DOCDIR}")
message(STATUS "CMAKE_INSTALL_MANDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}")

find_package(Threads REQUIRED)

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

# -----------------------

add_subdirectory(core)
add_subdirectory(lib)

# -----------------------

# Add the install targets
# Specify the directory on disk to which a file will be installed. <dir> should be a relative path. An absolute path is allowed, but not recommended.
# When a relative path is given it is interpreted relative to the value of the CMAKE_INSTALL_PREFIX variable.
install(TARGETS kamailio 
        DESTINATION ${CMAKE_INSTALL_SBINDIR}
        COMPONENT kamailio-core)

# Install the configuration file (kamailio.cfg) using a CODE block to check
# existence at install time instead of configure time
install(CODE "
    if(EXISTS \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/${MAIN_NAME}/kamailio.cfg\")
        message(STATUS \"kamailio.cfg already exists in \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/${MAIN_NAME}/kamailio.cfg. Installing as kamailio.cfg.sample\")
		file(INSTALL \"${CMAKE_SOURCE_DIR}/etc/kamailio.cfg\" 
		DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/${MAIN_NAME}\"
		RENAME \"kamailio.cfg.sample\" 
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
		)
    else()
        file(INSTALL \"${CMAKE_SOURCE_DIR}/etc/kamailio.cfg\" 
		DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/${MAIN_NAME}\"
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
" 
COMPONENT kamailio-cfg
)