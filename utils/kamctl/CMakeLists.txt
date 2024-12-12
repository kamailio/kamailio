cmake_minimum_required(VERSION 3.10)

project(kamailio_utils)

# if(NOT basedir) set(KAMCTL_TMP_DIR "/tmp") else() set(KAMCTL_TMP_DIR
# "${basedir}") endif()

set(KAMCTL_TMP_DIR "${CMAKE_BINARY_DIR}/kamctltmp")
message(STATUS "Temporary kamctl.{db} files in : ${KAMCTL_TMP_DIR}")

file(MAKE_DIRECTORY ${KAMCTL_TMP_DIR})

set(BIN_DIR "${CMAKE_INSTALL_FULL_SBINDIR}")
set(LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/kamctl")
set(CFG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}")
set(SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${MAIN_NAME}")

# Install kamctl
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl > ${KAMCTL_TMP_DIR}/kamctl
  COMMENT "Processed kamctl with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin
)

# Install kamctl.base
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl.base"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.base > ${KAMCTL_TMP_DIR}/kamctl.base
  COMMENT "Processed kamctl.base with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl.base
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

# Install kamctl.ctlbase
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl.ctlbase"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.ctlbase >
    ${KAMCTL_TMP_DIR}/kamctl.ctlbase
  COMMENT "Processed kamctl.ctlbase with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl.ctlbase
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

# Install kamctl.rpcfifo
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl.rpcfifo"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.rpcfifo >
    ${KAMCTL_TMP_DIR}/kamctl.rpcfifo
  COMMENT "Processed kamctl.rpcfifo with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl.rpcfifo
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

# Install kamctl.ser
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl.ser"
  COMMAND sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
          ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.ser > ${KAMCTL_TMP_DIR}/kamctl.ser
  COMMENT "Processed kamctl.ser with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl.ser
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

# Install kamctl.sqlbase
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl.sqlbase"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.sqlbase >
    ${KAMCTL_TMP_DIR}/kamctl.sqlbase
  COMMENT "Processed kamctl.sqlbase with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl.sqlbase
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

# Kamctl target to depend on all kamctl files
add_custom_target(
  kamctl ALL
  DEPENDS "${KAMCTL_TMP_DIR}/kamctl" "${KAMCTL_TMP_DIR}/kamctl.base"
          "${KAMCTL_TMP_DIR}/kamctl.ctlbase" "${KAMCTL_TMP_DIR}/kamctl.rpcfifo"
          "${KAMCTL_TMP_DIR}/kamctl.ser" "${KAMCTL_TMP_DIR}/kamctl.sqlbase"
)

# Install kamdbctl
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamdbctl"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl > ${KAMCTL_TMP_DIR}/kamdbctl
  COMMENT "Processed kamdbctl with sed"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamdbctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin
)
# Install db setup base script
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamdbctl.base"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" -e
    "s#/usr/local/share/kamailio#${SHARE_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl.base > ${KAMCTL_TMP_DIR}/kamdbctl.base
  COMMENT "Processed kamdbctl.base"
)
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamdbctl.base
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
  COMPONENT kamailio-utils-bin
)

add_custom_target(
  kamdbctl ALL DEPENDS "${KAMCTL_TMP_DIR}/kamdbctl"
                       "${KAMCTL_TMP_DIR}/kamdbctl.base"
)

# Install the configuration file (kamctlrc) using a CODE block to check
# existence at install time instead of configure time
install(
  CODE "
    if(EXISTS \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/kamctlrc\")
        message(STATUS \"kamctlrc already exists in \$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/kamctlrc. Installing as kamctlrc.sample\")
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/kamctlrc\"
          DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
          RENAME \"kamctlrc.sample\"
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    else()
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/kamctlrc\"
            DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
"
  COMPONENT kamailio-utils-cfg
)

# Custom target for installing the kamailio-cfg component
add_custom_target(
  install-kamailio-utils-cfg
  COMMAND ${CMAKE_COMMAND} --install . --component kamailio-utils-cfg
  COMMENT "Installing kamailio-utils-cfg component"
)

# Custom target for installing the kamailio-utils-bin component
add_custom_target(
  install-kamailio-utils-bin
  COMMAND ${CMAKE_COMMAND} --install . --component kamailio-utils-bin
  COMMENT "Installing kamailio-utils-bin component"
  DEPENDS kamctl kamdbctl
)

# Custom target for installing the kamailio-utils component
add_custom_target(
  install-kamailio-utils # COMMENT "Installing kamailio-utils-cfg and
                         # kamailio-utils-bin components"
  DEPENDS install-kamailio-utils-cfg install-kamailio-utils-bin
)
