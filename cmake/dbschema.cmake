# ----------
# dbschema target: Old Makefiles: make dbschema declared in src/ Makefile This
# was using the makefile found in src/lib/srdb1/schema folder.
#
# CMakeLists.txt in src/ includes this file. All modules that have a schema
# should be appended to the end of this file. See the the rest of modules.
find_program(XSLTPROC_EXECUTABLE xsltproc QUIET)

include(${CMAKE_SOURCE_DIR}/cmake/groups.cmake)
get_property(added_modules GLOBAL PROPERTY ADDED_MODULES_LIST)
# message(WARNING "Added modules: ${added_modules}")

# Function to add a target for each database type prefix with dbschema ie
# db_name = redis -> target = dbschema_redis
function(add_db_target db_name xsl_file)
  if(NOT XSLTPROC_EXECUTABLE)
    return()
  endif()
  # Change name for the folder
  if(db_name STREQUAL "pi_framework_table" OR db_name STREQUAL "pi_framework_mod")
    set(db_name_folder xhttp_pi)
  else()
    set(db_name_folder ${db_name})
  endif()

  if(NOT (db_name_folder IN_LIST added_modules))
    return()
  endif()

  find_group_name(${db_name_folder})

  add_custom_target(
    dbschema_${db_name}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${db_name_folder}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/utils/kamctl
    COMMENT "Creating schemas for ${db_name}"
  )

  # db_name for old makefiles are different.
  #     old       ->     new (module names)
  # mongodb       -> db_mongodb
  # mysql         -> db_mysql
  # db_oracle     -> db_oracle
  # postgres      -> db_postgres
  # db_redis      -> db_redis
  # db_sqlite     -> db_sqlite
  # dbtext        -> db_text
  # pi_framework  -> xhttp_pi (this was not provided at all in the old makefiles)
  # For consistency, we are now using the new names.
  # For compatibility with tools, we are still using the old names for install folder

  # install folder based on db_name
  if(db_name STREQUAL "db_mongodb")
    set(install_folder "mongodb")
  elseif(db_name STREQUAL "db_mysql")
    set(install_folder "mysql")
  elseif(db_name STREQUAL "db_postgres")
    set(install_folder "postgres")
  elseif(db_name STREQUAL "db_text")
    set(install_folder "dbtext")
  elseif(db_name STREQUAL "pi_framework_table" OR db_name STREQUAL "pi_framework_mod")
    set(install_folder "xhttp_pi")
  else()
    set(install_folder "${db_name}")
  endif()

  # Loop through each table and add a command for xsltproc
  foreach(table ${EXTRACTED_TABLES})

    # Determine the prefix/suffix
    if(db_name STREQUAL "db_redis"
       OR db_name STREQUAL "db_text"
       OR db_name STREQUAL "db_mongodb"
    )
      set(prefix '')
      set(folder_suffix "${MAIN_NAME}")
    else()
      set(prefix "${table}-")
      set(folder_suffix '')
    endif()

    # Stringparam db is the db_* module name
    add_custom_command(
      TARGET dbschema_${db_name}
      PRE_BUILD
      COMMAND
        "XML_CATALOG_FILES=${CATALOG}" ${XSLTPROC_EXECUTABLE} ${XSLTPROC_FLAGS} --stringparam dir
        ${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}/${folder_suffix} --stringparam prefix
        ${prefix} --stringparam db ${db_name} ${xsl_file} "kamailio-${table}.xml"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/lib/srdb1/schema"
      COMMENT "Processing ${table} for ${db_name}"
    )
    # Ensure the generated files are cleaned up by the global clean target
    # Note: this only works for CMake >= 3.15 and only for Make and Ninja generators
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.15")
      set_target_properties(
        dbschema_${db_name} PROPERTIES ADDITIONAL_CLEAN_FILES
                                       "${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}/"
      )
    endif()
  endforeach()

  # Create version table for db_text
  # Use bash script
  set(POSTPROCESS_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/dbschema-version-postprocess.sh")

  if(db_name STREQUAL "db_text")
    add_custom_command(
      TARGET dbschema_${db_name}
      POST_BUILD
      COMMAND ${POSTPROCESS_SCRIPT} 1 1
      COMMENT "Creating version table for ${db_name}"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}/${folder_suffix}
    )
  endif()

  add_custom_target(
    dbschema_${db_name}_clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory
            "${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}"
    COMMENT "Cleaning ${db_name} schema files"
  )

  add_dependencies(dbschema dbschema_${db_name})
  add_dependencies(dbschema_clean dbschema_${db_name}_clean)

  # message(WARNING "group name is ${group_name}")
  # Before installing, ensure the target is built `dbschema_${db_name}`
  # install as previously done in makefile folder. see naming above
  # TODO: when tools adopt to new folder structure, replace the install_folder variable
  # with ${db_name_folder}

  install(
    DIRECTORY ${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/${install_folder}
    OPTIONAL
    COMPONENT ${group_name}
  )
endfunction()

if(NOT XSLTPROC_EXECUTABLE)
  message(STATUS "xsltproc is not found. Skip dbschema target.")
else()
  #  Add targets for each database type
  if(NOT TARGET dbschema)
    add_custom_target(dbschema ALL COMMENT "Generating schemas for all dbs...")
  endif()
  if(NOT TARGET dbschema_clean)
    add_custom_target(dbschema_clean COMMENT "Cleaning schemas for all dbs...")
  endif()

  option(XSLT_VALIDATE "Enable schema validation during XSL transformations" ON)
  option(XSLT_VERBOSE "Enable verbose output for XSL transformations" OFF)

  set(XSLTPROC_FLAGS --xinclude)
  if(NOT ${XSLT_VALIDATE})
    set(XSLTPROC_FLAGS ${XSLTPROC_FLAGS} --novalid)
  endif()
  if(${XSLT_VERBOSE})
    set(XSLTPROC_FLAGS ${XSLTPROC_FLAGS} --verbose)
  endif()

  # Set the root directories
  set(ROOTDIR ${CMAKE_SOURCE_DIR})
  set(STYLESHEETS ${ROOTDIR}/doc/stylesheets/dbschema_k/xsl)
  set(CATALOG ${ROOTDIR}/doc/stylesheets/dbschema_k/catalog.xml)

  # List of XML files
  file(GLOB TABLES "${CMAKE_SOURCE_DIR}/src/lib/srdb1/schema/kamailio-*.xml")
  # message(WARNING "TABLES : ${TABLES}")
  set(EXTRACTED_TABLES "")
  foreach(table ${TABLES})
    get_filename_component(TABLE_NAME "${table}" NAME)
    string(REPLACE "kamailio-" "" TABLE_NAME "${TABLE_NAME}")
    string(REPLACE ".xml" "" TABLE_NAME "${TABLE_NAME}")
    list(APPEND EXTRACTED_TABLES "${TABLE_NAME}")
    list(SORT EXTRACTED_TABLES)
  endforeach()
  # Output the extracted table names
  if(VERBOSE)
    message(STATUS "Extracted Tables for DB schema generation: ${EXTRACTED_TABLES}")
  endif()
endif()

#---- DB mongo
add_db_target(db_mongodb "${STYLESHEETS}/db_mongodb.xsl")
# Create the version-create.mongo script
# After processing the JSON files, create the version-create.mongo script
# Usage of generate_version_create_mongo.sh:
# 1. The first argument is the path to create the version-create.mongo script
# 2. The second argument is the path to the directory containing the JSON files
if(TARGET dbschema_db_mongodb)
  add_custom_command(
    TARGET dbschema_db_mongodb
    POST_BUILD
    COMMAND
      sh generate_version_create_mongo.sh
      "${CMAKE_BINARY_DIR}/utils/kamctl/db_mongodb/kamailio/version-create.mongo"
      "${CMAKE_BINARY_DIR}/utils/kamctl/db_mongodb/kamailio"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/utils/kamctl
    COMMENT "Creating version-create.mongo from JSON files"
  )

  find_group_name("db_mongodb")
  install(
    FILES ${CMAKE_BINARY_DIR}/utils/kamctl/db_mongodb/kamailio/version-create.mongo
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/mongodb/${MAIN_NAME}
    OPTIONAL
    COMPONENT ${group_name}
  )
endif()

#---- DB mysql
add_db_target(db_mysql "${STYLESHEETS}/db_mysql.xsl")

#---- DB Oracle
add_db_target(db_oracle "${STYLESHEETS}/db_oracle.xsl")

#---- DB postgres
add_db_target(db_postgres "${STYLESHEETS}/db_postgres.xsl")

#---- DB redis
add_db_target(db_redis "${STYLESHEETS}/db_redis.xsl")

#---- DB sqlite
add_db_target(db_sqlite "${STYLESHEETS}/db_sqlite.xsl")

#---- DB text
add_db_target(db_text "${STYLESHEETS}/db_text.xsl")

#---- DB xhttp_pi
add_db_target(pi_framework_table "${STYLESHEETS}/pi_framework_table.xsl")
add_db_target(pi_framework_mod "${STYLESHEETS}/pi_framework_mod.xsl")

# Add alias targets that match the dbschema
if(XSLTPROC_EXECUTABLE
   AND TARGET dbschema_pi_framework_table
   AND TARGET dbschema_pi_framework_mod
)
  add_custom_target(dbschema_xhttp_pi)
  add_dependencies(dbschema_xhttp_pi dbschema_pi_framework_table dbschema_pi_framework_mod)

  add_custom_target(dbschema_xhttp_pi_clean)
  add_dependencies(
    dbschema_xhttp_pi dbschema_pi_framework_table_clean dbschema_pi_framework_mod_clean
  )

  add_dependencies(dbschema dbschema_pi_framework_table dbschema_pi_framework_mod)
  add_dependencies(dbschema_clean dbschema_pi_framework_table_clean dbschema_pi_framework_mod_clean)
endif()
