# ----------
# dbschema target: Old Makefiles: make dbschema declared in src/ Makefile This
# was using the makefile found in src/lib/srdb1/schema folder.
#
# CMakeLists.txt in src/ includes this file. All modules that have a schema
# should be appended to the end of this file. See the the rest of modules.
find_program(XSLTPROC_EXECUTABLE xsltproc QUIET)

# Function to add a target for each database type prefix with dbschema ie
# db_name = redis -> target = dbschema_redis
function(add_db_target group_name db_name xsl_file)
  if(NOT XSLTPROC_EXECUTABLE)
    return()
  endif()
  # Change name for the folder
  if(db_name STREQUAL "pi_framework_table" OR db_name STREQUAL "pi_framework_mod")
    set(db_name_folder xhttp_pi)
  else()
    set(db_name_folder ${db_name})
  endif()

  add_custom_target(
    dbschema_${db_name}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${db_name_folder}"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/utils/kamctl
    COMMENT "Creating schemas for ${db_name}"
  )

  # Loop through each table and add a command for xsltproc
  foreach(table ${EXTRACTED_TABLES})
    # Determine the prefix based on db_name
    if(db_name STREQUAL "db_berkeley"
       OR db_name STREQUAL "db_redis"
       OR db_name STREQUAL "dbtext"
       OR db_name STREQUAL "mongodb"
    )
      set(prefix '')
      set(folder_suffix "${MAIN_NAME}")
    else()
      set(prefix "${table}-")
      set(folder_suffix '')
    endif()

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
  endforeach()

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
  install(
    DIRECTORY ${CMAKE_BINARY_DIR}/utils/kamctl/${db_name_folder}
    DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}
    OPTIONAL
    COMPONENT ${group_name}
  )
endfunction()

if(NOT XSLTPROC_EXECUTABLE)
  message(STATUS "xsltproc is not found. Skip dbschema target.")
else()
  #  Add targets for each database type
  if(NOT TARGET dbschema)
    add_custom_target(dbschema COMMENT "Generating schemas for all dbs...")
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
  endforeach()
  # Output the extracted table names
  if(VERBOSE)
    message(STATUS "Extracted Tables for DB schema generation: ${EXTRACTED_TABLES}")
  endif()
endif()

#---- DB berkeley
add_db_target("${group_name}" db_berkeley "${STYLESHEETS}/db_berkeley.xsl")

#---- DB monogo
add_db_target("${group_name}" mongodb "${STYLESHEETS}/mongodb.xsl")
# Create the version-create.mongo script
# After processing the JSON files, create the version-create.mongo script
# Usage of generate_version_create_mongo.sh:
# 1. The first argument is the path to create the version-create.mongo script
# 2. The second argument is the path to the directory containing the JSON files
if(TARGET dbschema_mongodb)
  add_custom_command(
    TARGET dbschema_mongodb
    POST_BUILD
    COMMAND
      bash generate_version_create_mongo.sh
      "${CMAKE_BINARY_DIR}/utils/kamctl/mongodb/kamailio/version-create.mongo"
      "${CMAKE_BINARY_DIR}/utils/kamctl/mongodb/kamailio"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/utils/kamctl
    COMMENT "Creating version-create.mongo from JSON files"
  )
endif()

install(
  FILES ${CMAKE_BINARY_DIR}/utils/kamctl/mongodb/kamailio/version-create.mongo
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/mongodb/${MAIN_NAME}
  OPTIONAL
  COMPONENT ${group_name}
)

#---- DB mysql
add_db_target("${group_name}" mysql "${STYLESHEETS}/mysql.xsl")

#---- DB Oracle
add_db_target("${group_name}" db_oracle "${STYLESHEETS}/oracle.xsl")

#---- DB postgres
add_db_target("${group_name}" postgres "${STYLESHEETS}/postgres.xsl")

#---- DB redis
add_db_target("${group_name}" db_redis "${STYLESHEETS}/db_redis.xsl")

#---- DB sqlite
add_db_target("${group_name}" db_sqlite "${STYLESHEETS}/db_sqlite.xsl")

#---- DB text
add_db_target("${group_name}" dbtext "${STYLESHEETS}/dbtext.xsl")

#---- DB xhttp_pi
add_db_target("${group_name}" pi_framework_table "${STYLESHEETS}/pi_framework_table.xsl")
add_db_target("${group_name}" pi_framework_mod "${STYLESHEETS}/pi_framework_mod.xsl")

# Add alias targets that match the dbschema
if(XSLTPROC_EXECUTABLE)
  add_custom_target(dbschema_xhttp_pi)
  add_dependencies(dbschema_xhttp_pi dbschema_pi_framework_table dbschema_pi_framework_mod)

  add_custom_target(dbschema_xhttp_pi_clean)
  add_dependencies(
    dbschema_xhttp_pi dbschema_pi_framework_table_clean dbschema_pi_framework_mod_clean
  )

  add_dependencies(dbschema dbschema_pi_framework_table dbschema_pi_framework_mod)
  add_dependencies(dbschema_clean dbschema_pi_framework_table_clean dbschema_pi_framework_mod_clean)
endif()
