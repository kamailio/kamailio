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
pkg_check_modules(READLINE IMPORTED_TARGET readline)

if(READLINE_FOUND)
  add_library(readline::readline ALIAS PkgConfig::READLINE)
  target_compile_definitions(${NAME} PRIVATE USE_READLINE)
  target_link_libraries(${NAME} PRIVATE readline::readline)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

target_sources(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kamcmd.c
                               ${CMAKE_CURRENT_SOURCE_DIR}/parse_listen_id.c)
# target_include_directories(${NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# target_link_libraries(${NAME} PRIVATE common)
target_link_libraries(${NAME} PRIVATE resolv)
target_link_libraries(${NAME} PRIVATE common_utils)

target_compile_definitions(
  ${NAME}
  PRIVATE NAME="${NAME}" SRNAME="${MAIN_NAME}" VERSION="${KAMCMD_RELEASE}"
          RUN_DIR="/${CMAKE_INSTALL_LOCALSTATEDIR}/run/${MAIN_NAME}"
          __OS_${OS_LOWER})

install(TARGETS ${NAME} DESTINATION ${CMAKE_INSTALL_SBINDIR})

# Man page generation
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kamcmd.8
  COMMAND
    sed -e
    "s#/etc/${CFG_NAME}/${CFG_NAME}\.cfg#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/${MAIN_NAME}.cfg#g"
    -e "s#/usr/sbin/#${CMAKE_INSTALL_FULL_SBINDIR}/#g" -e
    "s#/usr/lib/${CFG_NAME}/modules/#${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/#g"
    -e "s#/usr/share/doc/${CFG_NAME}/#${CMAKE_INSTALL_FULL_DOCDIR}/#g" -e
    "s#$(SRC_NAME)#$(MAIN_NAME)#g" < ${CMAKE_CURRENT_SOURCE_DIR}/kamcmd.8 >
    ${CMAKE_CURRENT_BINARY_DIR}/kamcmd.8
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kamcmd.8)

add_custom_target(kamcmd_man DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kamcmd.8)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/kamcmd.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
  COMPONENT kamailio_man
  OPTIONAL)
