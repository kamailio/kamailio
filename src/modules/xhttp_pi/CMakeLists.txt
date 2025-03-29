file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(LibXml2 REQUIRED)

target_link_libraries(${module_name} PRIVATE LibXml2::LibXml2)

# Install the configuration file (tls.cfg) using a CODE block to check
# existence at install time instead of configure time
install(
  CODE "
    set(dir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\")
    set(file \"pi_framework.xml\")
    if(EXISTS \"\${dir}/\${file}\")
        message(STATUS \"\${file} already exists in \${dir}/\${file}.
          Installing as \${file}.sample\")
        file(INSTALL \"${CMAKE_SOURCE_DIR}/misc/examples/pi_framework/\${file}\"
          DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
          RENAME \"\${file}.sample\"
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    else()
        file(INSTALL \"${CMAKE_SOURCE_DIR}/misc/examples/pi_framework/\${file}\"
            DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
"
  COMPONENT xhttp_pi-cfg)

add_custom_target(
  xhttp_pi-install-cfg
  COMMENT "Installing pi_framework.xml"
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component
          xhttp_pi-cfg)
