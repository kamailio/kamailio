set(KAMCMD_RELEASE 1.5)
set(KAMCMD_UTIL_SRC_NAME "kamcmd")

set(KAMCMD_CUSTOM_NAME
    ""
    CACHE STRING "Custom name for the kamcmd util")

if(NOT KAMCMD_CUSTOM_NAME)
  set(NAME "kamcmd") # Hardcoded name if empty
elseif()
  set(NAME "${KAMCMD_CUSTOM_NAME}")
endif()

add_executable(${NAME})

# detect libreadline
find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED IMPORTED_TARGET readline)
add_library(readline::readline ALIAS PkgConfig::READLINE)

if(READLINE_FOUND)
  target_compile_definitions(${NAME} PRIVATE USE_READLINE)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

target_sources(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kamcmd.c
                               ${CMAKE_CURRENT_SOURCE_DIR}/parse_listen_id.c)
# target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# target_link_libraries(${NAME} PRIVATE common)
target_link_libraries(${NAME} PRIVATE readline::readline resolv)
target_link_libraries(${NAME} PRIVATE common_utils)

target_compile_definitions(
  ${NAME}
  PRIVATE NAME="${NAME}" SRNAME="${MAIN_NAME}" VERSION="${KAMCMD_RELEASE}"
          RUN_DIR="/${CMAKE_INSTALL_LOCALSTATEDIR}/run/${MAIN_NAME}"
          __OS_${OS_LOWER})

install(TARGETS ${NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})
