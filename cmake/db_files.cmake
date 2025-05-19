function(add_db_files group_name file kamctl)
  # message(WARNING "file name is ${file}")
  # message(WARNING "group name is ${group_name}")
  # Process the file with sed and install it
  add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/utils/kamctl/${file}"
    COMMAND sed -e "s#/usr/local/sbin#${BIN_DIR}#g" < ${CMAKE_SOURCE_DIR}/utils/kamctl/${file} >
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

function(add_kamctl_db_files group_name file)
  add_db_files(${group_name} ${file} 1)
endfunction()

function(add_kamdbctl_db_files group_name file)
  add_db_files(${group_name} ${file} 0)
endfunction()
