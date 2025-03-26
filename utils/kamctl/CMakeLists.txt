project(kamailio_utils)

get_property(FINAL_MODULES_LIST GLOBAL PROPERTY ADDED_MODULES_LIST)

set(BIN_DIR "${CMAKE_INSTALL_FULL_SBINDIR}")
set(LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}")
set(CFG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}")
set(SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${MAIN_NAME}")

include(${CMAKE_SOURCE_DIR}/cmake/db_files.cmake)

# --- kamctl  ---
set(KAMCTL_BASE_FILES kamctl.base kamctl.ctlbase kamctl.rpcfifo kamctl.ser kamctl.sqlbase)

# Install kamctl.base and the rest
foreach(file IN LISTS KAMCTL_BASE_FILES)
  add_kamctl_db_files("kamailio-core" ${file})
endforeach()

# Kamctl target to depend on all kamctl files
# Install kamctl and it's database files
get_property(KAMCTL_DEPENDENCIES GLOBAL PROPERTY KAMCTL_DEPENDENCIES)
add_custom_target(
  kamctl ALL
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" < ${CMAKE_CURRENT_SOURCE_DIR}/kamctl >
    ${CMAKE_CURRENT_BINARY_DIR}/kamctl
  COMMENT "Build kamctl and its dependencies"
)
add_dependencies(kamctl ${KAMCTL_DEPENDENCIES})

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/kamctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-core
)

# --- kamdbctl  ---
# Setup and install kamdbctl stuff
set(KAMDBCTL_BASE_FILES kamdbctl.base)

# Install kamdbctl
foreach(file IN LISTS KAMDBCTL_BASE_FILES)
  add_kamdbctl_db_files("kamailio-core" ${file})
endforeach()

get_property(KAMDBCTL_DEPENDENCIES GLOBAL PROPERTY KAMDBCTL_DEPENDENCIES)
add_custom_target(
  kamdbctl ALL
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" < ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl >
    ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl
  COMMENT "Build kamdbctl and its dependencies"
)
add_dependencies(kamdbctl ${KAMDBCTL_DEPENDENCIES})

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-core
)

# Install the configuration file (kamctlrc) using a CODE block to check
# existence at install time instead of configure time
install(
  CODE "
    set(dir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\")

    if(EXISTS \"\${dir}/kamctlrc\")
        message(STATUS \"kamctlrc already exists in\${dir}/kamctlrc.
          Installing as kamctlrc.sample\")
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
  COMPONENT kamailio-core
)

# Man pages generation and installation
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
  COMMAND
    sed -e
    "s#/etc/${CFG_NAME}/${CFG_NAME}\.cfg#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/${MAIN_NAME}.cfg#g"
    -e "s#/usr/sbin/#${CMAKE_INSTALL_FULL_SBINDIR}/#g" -e
    "s#/usr/lib/${CFG_NAME}/modules/#${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/#g" -e
    "s#/usr/share/doc/${CFG_NAME}/#${CMAKE_INSTALL_FULL_DOCDIR}/#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.8 > ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.8
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  COMMAND
    sed -e
    "s#/etc/${CFG_NAME}/${CFG_NAME}\.cfg#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/${MAIN_NAME}.cfg#g"
    -e "s#/usr/sbin/#${CMAKE_INSTALL_FULL_SBINDIR}/#g" -e
    "s#/usr/lib/${CFG_NAME}/modules/#${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/#g" -e
    "s#/usr/share/doc/${CFG_NAME}/#${CMAKE_INSTALL_FULL_DOCDIR}/#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl.8 > ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl.8
)

add_custom_target(
  kamctl_man DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8 ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
)

# In order for these files to be installed, one has to built the kamctl_man target
install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8 ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
  COMPONENT kamailio-core
  OPTIONAL
)

# Install targets
# TODO: FIX IF POSSIBLE after component based-installed components do not exist
# Custom target for installing the kamailio-cfg component
add_custom_target(
  install-kamailio-utils-cfg
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component kamailio-utils-cfg
  COMMENT "Installing kamailio-utils-cfg component"
)

# Custom target for installing the kamailio-utils-bin component
add_custom_target(
  install-kamailio-utils-bin
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component kamailio-utils-bin
  COMMENT "Installing kamailio-utils-bin component"
  DEPENDS kamctl kamdbctl
)

# Custom target for installing the kamailio-utils component
add_custom_target(install-kamailio-utils COMMENT "Installing utils components")
add_dependencies(install-kamailio-utils install-kamailio-utils-cfg install-kamailio-utils-bin)
