file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(Radius REQUIRED)

target_compile_definitions(${module_name} PRIVATE ${RADIUSCLIENT_LIB})
target_link_libraries(${module_name} PRIVATE RadiusClient::RadiusClient)

# Install the configuration file (tls.cfg) using a CODE block to check
# existence at install time instead of configure time
install(
  CODE "
    set(dir \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\")
    set(file \"dictionary.${CFG_NAME}\")
    if(EXISTS \"\${dir}/\${file}\")
        message(STATUS \"\${file} already exists in \${dir}/\${file}.
          Installing as \${file}.sample\")
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/cfg/\${file}\"
          DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
          RENAME \"\${file}.sample\"
          PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        )
    else()
        file(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/cfg/\${file}\"
            DESTINATION \"${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}\"
            PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    endif()
"
  COMPONENT ${module_name}-cfg
)

add_custom_target(
  ${module_name}-install-cfg
  COMMENT "Installing auth_radius dictionary.${CFG_NAME} configuration file"
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component ${module_name}-cfg
)
