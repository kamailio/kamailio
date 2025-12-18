# Funtion to add db files to the target kamctl/kamdbctl
# It takes the group name, the file name and a boolean to determine if it is for kamctl or kamdbctl
# It processes the file with sed and installs it to the correct location
# Used by the helper function add_kamctl_db_files and add_kamdbctl_db_files

function(add_db_files group_name file kamctl)
  # message(WARNING "file name is ${file}")
  # message(WARNING "group name is ${group_name}")
  # Process the file with sed and install it
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/utils/kamctl/${file}"
    COMMAND
      sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e "s#/usr/local/share/kamailio#${SHARE_DIR}#g" -e
      "s#/usr/local/etc/kamailio#${CFG_DIR}#g" < ${CMAKE_SOURCE_DIR}/utils/kamctl/${file} >
      ${CMAKE_BINARY_DIR}/utils/kamctl/${file}
    COMMENT "Processed ${file} with sed "
  )

  # Append to the depependencies list for the target kamctl/kamdbctl respectively
  if(kamctl)
    add_custom_target(
      kamctl_${file}
      DEPENDS ${CMAKE_BINARY_DIR}/utils/kamctl/${file}
      COMMENT "Generating kamctl_${file}"
    )
    set_property(GLOBAL APPEND PROPERTY KAMCTL_DEPENDENCIES "kamctl_${file}")
  else()
    add_custom_target(
      kamdbctl_${file}
      DEPENDS ${CMAKE_BINARY_DIR}/utils/kamctl/${file}
      COMMENT "Generating kamctl_${file}"
    )
    set_property(GLOBAL APPEND PROPERTY KAMDBCTL_DEPENDENCIES "kamdbctl_${file}")
  endif()
  install(
    PROGRAMS ${CMAKE_BINARY_DIR}/utils/kamctl/${file}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT ${group_name}
  )
endfunction()

# Helper functions to add kamctl releated db files
# Used by utils/kamctl/CMakeLists.txt for the core kamctl files
# and by modules/db_{module_name}/CMakeLists.txt for the module specific kamctl files
function(add_kamctl_db_files group_name file)
  add_db_files(${group_name} ${file} 1)
endfunction()

# Helper functions to add kamdbctl releated db files
# Used by utils/kamctl/CMakeLists.txt for the core kamctl files
# and by modules/db_{module_name}/CMakeLists.txt for the module specific kamdbctl files
function(add_kamdbctl_db_files group_name file)
  add_db_files(${group_name} ${file} 0)
endfunction()
