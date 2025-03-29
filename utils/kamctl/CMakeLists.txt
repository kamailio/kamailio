project(kamailio_utils)

get_property(FINAL_MODULES_LIST GLOBAL PROPERTY ADDED_MODULES_LIST)

set(BIN_DIR "${CMAKE_INSTALL_FULL_SBINDIR}")
set(LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}")
set(CFG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}")
set(SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${MAIN_NAME}")

set(KAMCTL_BASE_FILES kamctl.base kamctl.ctlbase kamctl.rpcfifo kamctl.ser
                      kamctl.sqlbase)

if("db_berkeley" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.db_berkeley)
endif()
if("db_text" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.dbtext)
endif()
if("db_mysql" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.mysql)
endif()
if("db_oracle" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.oracle)
endif()
if("db_postgres" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.pgsql)
endif()
if("db_sqlite" IN_LIST FINAL_MODULES_LIST)
  set(KAMCTL_DB_FILES ${KAMCTL_DB_FILES} kamctl.sqlite)
endif()

# Create the depependencies list for the target kamctl
set(KAMCTL_DEPENDENCIES "")

# Install kamctl.base and the rest
foreach(file IN LISTS KAMCTL_BASE_FILES KAMCTL_DB_FILES)
  # Append to the depependencies list for the target kamctl
  list(APPEND KAMCTL_DEPENDENCIES "${CMAKE_CURRENT_BINARY_DIR}/${file}")
  # Process the file with sed and install it
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${file}"
    COMMAND
      sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
      ${CMAKE_CURRENT_SOURCE_DIR}/${file} > ${CMAKE_CURRENT_BINARY_DIR}/${file}
    COMMENT "Processed ${file} with sed")
  install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${file}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

# Kamctl target to depend on all kamctl files
# Install kamctl and it's database files
add_custom_target(
  kamctl ALL
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl > ${CMAKE_CURRENT_BINARY_DIR}/kamctl
  DEPENDS ${KAMCTL_DEPENDENCIES}
  COMMENT "Build kamctl and its dependencies")

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/kamctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin)

# Setup and install kamdbctl stuff
set(KAMDBCTL_BASE_FILES kamdbctl.base)

if("db_berkeley" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.db_berkeley)
endif()
if("db_text" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.dbtext)
  install(PROGRAMS dbtextdb/dbtextdb.py
          DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl/dbtextdb)
endif()
if("db_mysql" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.mysql)
endif()
if("db_oracle" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.oracle kamdbfunc.oracle)
endif()
if("db_postgres" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.pgsql)
endif()
if("db_sqlite" IN_LIST FINAL_MODULES_LIST)
  set(KAMDBCTL_DB_FILES ${KAMDBCTL_DB_FILES} kamdbctl.sqlite)
endif()

# Install kamdbctl
set(KAMDB_DEPENDENCIES "")

foreach(file IN LISTS KAMDBCTL_BASE_FILES KAMDBCTL_DB_FILES)
  list(APPEND KAMDB_DEPENDENCIES "${CMAKE_CURRENT_BINARY_DIR}/${file}")
  # Install db setup base script
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${file}"
    COMMAND
      sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
      "s#/usr/local/etc/kamailio#${CFG_DIR}#g" -e
      "s#/usr/local/share/kamailio#${SHARE_DIR}#g" <
      ${CMAKE_CURRENT_SOURCE_DIR}/${file} > ${CMAKE_CURRENT_BINARY_DIR}/${file}
    COMMENT "Processed ${file}")
  install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/${file}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

add_custom_target(
  kamdbctl ALL
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl > ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl
  DEPENDS ${KAMDB_DEPENDENCIES}
  COMMENT "Build kamdbctl and its dependencies")

install(
  PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin)

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
  COMPONENT kamailio-utils-cfg)

# Man pages generation and installation
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
  COMMAND
    sed -e
    "s#/etc/${CFG_NAME}/${CFG_NAME}\.cfg#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/${MAIN_NAME}.cfg#g"
    -e "s#/usr/sbin/#${CMAKE_INSTALL_FULL_SBINDIR}/#g" -e
    "s#/usr/lib/${CFG_NAME}/modules/#${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/#g"
    -e "s#/usr/share/doc/${CFG_NAME}/#${CMAKE_INSTALL_FULL_DOCDIR}/#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.8 > ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kamctl.8)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  COMMAND
    sed -e
    "s#/etc/${CFG_NAME}/${CFG_NAME}\.cfg#${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}/${MAIN_NAME}.cfg#g"
    -e "s#/usr/sbin/#${CMAKE_INSTALL_FULL_SBINDIR}/#g" -e
    "s#/usr/lib/${CFG_NAME}/modules/#${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}/#g"
    -e "s#/usr/share/doc/${CFG_NAME}/#${CMAKE_INSTALL_FULL_DOCDIR}/#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl.8 >
    ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl.8)

add_custom_target(kamctl_man DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
                                     ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8
        ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
  DESTINATION ${CMAKE_INSTALL_MANDIR}/man8
  COMPONENT kamailio_man
  OPTIONAL)

# Install targets
# Custom target for installing the kamailio-cfg component
add_custom_target(
  install-kamailio-utils-cfg
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component
          kamailio-utils-cfg
  COMMENT "Installing kamailio-utils-cfg component")

# Custom target for installing the kamailio-utils-bin component
add_custom_target(
  install-kamailio-utils-bin
  COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component
          kamailio-utils-bin
  COMMENT "Installing kamailio-utils-bin component"
  DEPENDS kamctl kamdbctl)

# Custom target for installing the kamailio-utils component
add_custom_target(install-kamailio-utils COMMENT "Installing utils components")
add_dependencies(install-kamailio-utils install-kamailio-utils-cfg
                 install-kamailio-utils-bin)

# ----------

# ----------
# dbschema target: Old Makefiles: make dbschema declared in src/ Makefile This
# was using the makefile found in src/lib/srdb1/schema folder.
#
# TODO: Revise where we want the target declared. New CMakelists.txt Declare the
# target in srdb1 CMakeLists.txt file.

find_program(XSLTPROC_EXECUTABLE xsltproc QUIET)

if(NOT XSLTPROC_EXECUTABLE)
  message(STATUS "xsltproc is not found. Skip dbschema target.")
else()
  option(XSLT_VALIDATE, "Enable schema validation during XSL transformations"
         ON)
  option(XSLT_VERBOSE, "Enable verbose output for XSL transformations" OFF)

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
    message(
      STATUS "Extracted Tables for DB schema generation: ${EXTRACTED_TABLES}")
  endif()

  # Function to add a target for each database type prefix with dbschema ie
  # db_name = redis -> target = dbschema_redis
  function(add_db_target db_name xsl_file)
    # Change name for the folder
    if(db_name STREQUAL "pi_framework_table" OR db_name STREQUAL
                                                "pi_framework_mod")
      set(db_name_folder xhttp_pi)
    else()
      set(db_name_folder ${db_name})
    endif()

    add_custom_target(
      dbschema_${db_name}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${db_name_folder}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Creating schemas for ${db_name}")

    # Loop through each table and add a command for xsltproc
    foreach(table ${EXTRACTED_TABLES})
      # Determine the prefix based on db_name
      if(db_name STREQUAL "db_berkeley"
         OR db_name STREQUAL "db_redis"
         OR db_name STREQUAL "dbtext"
         OR db_name STREQUAL "mongodb")
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
          XML_CATALOG_FILES=${CATALOG} ${XSLTPROC_EXECUTABLE} ${XSLTPROC_FLAGS}
          --stringparam dir
          ${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}/${folder_suffix}
          --stringparam prefix ${prefix} --stringparam db ${db_name} ${xsl_file}
          "kamailio-${table}.xml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/lib/srdb1/schema"
        COMMENT "Processing ${table} for ${db_name}")
    endforeach()

    # Before installing, ensure the target is built `dbschema_${db_name}`
    install(
      DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}
      DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}
      OPTIONAL)

    add_custom_target(
      dbschema_${db_name}_clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory
              "${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}"
      COMMENT "Cleaning ${db_name} schema files")
  endfunction()

  # Add targets for each database type
  add_custom_target(dbschema COMMENT "Generating schemas for all dbs...")
  add_custom_target(dbschema_clean COMMENT "Cleaning schemas for all dbs...")

  # Pi framework targets should combined into one
  if("xhttp_pi" IN_LIST FINAL_MODULES_LIST)
    add_db_target(pi_framework_table "${STYLESHEETS}/pi_framework_table.xsl")
    add_db_target(pi_framework_mod "${STYLESHEETS}/pi_framework_mod.xsl")
    add_custom_target(dbschema_xhttp_pi)
    add_dependencies(dbschema_xhttp_pi dbschema_pi_framework_table
                     dbschema_pi_framework_mod)

    add_custom_target(dbschema_xhttp_pi_clean)
    add_dependencies(dbschema_xhttp_pi dbschema_pi_framework_table_clean
                     dbschema_pi_framework_mod_clean)

    add_dependencies(dbschema dbschema_pi_framework_table
                     dbschema_pi_framework_mod)
    add_dependencies(dbschema_clean dbschema_pi_framework_table_clean
                     dbschema_pi_framework_mod_clean)
  endif()
  if("db_mysql" IN_LIST FINAL_MODULES_LIST)
    add_db_target(mysql "${STYLESHEETS}/mysql.xsl")
    add_dependencies(dbschema dbschema_mysql)
    add_dependencies(dbschema_clean dbschema_mysql_clean)
  endif()
  if("db_postgres" IN_LIST FINAL_MODULES_LIST)
    add_db_target(postgres "${STYLESHEETS}/postgres.xsl")
    add_dependencies(dbschema dbschema_postgres)
    add_dependencies(dbschema_clean dbschema_postgres_clean)
  endif()
  if("db_oracle" IN_LIST FINAL_MODULES_LIST)
    add_db_target(oracle "${STYLESHEETS}/oracle.xsl")
    add_dependencies(dbschema dbschema_oracle)
    add_dependencies(dbschema_clean dbschema_oracle_clean)
  endif()
  if("db_text" IN_LIST FINAL_MODULES_LIST)
    add_db_target(dbtext "${STYLESHEETS}/dbtext.xsl")
    add_dependencies(dbschema dbschema_dbtext)
    add_dependencies(dbschema_clean dbschema_dbtext_clean)
  endif()
  if("db_berkeley" IN_LIST FINAL_MODULES_LIST)
    add_db_target(db_berkeley "${STYLESHEETS}/db_berkeley.xsl")
    add_dependencies(dbschema dbschema_db_berkeley)
    add_dependencies(dbschema_clean dbschema_db_berkeley_clean)
  endif()
  if("db_sqlite" IN_LIST FINAL_MODULES_LIST)
    add_db_target(db_sqlite "${STYLESHEETS}/db_sqlite.xsl")
    add_dependencies(dbschema dbschema_db_sqlite)
    add_dependencies(dbschema_clean dbschema_db_sqlite_clean)
  endif()
  if("db_mongodb" IN_LIST FINAL_MODULES_LIST)
    add_db_target(mongodb "${STYLESHEETS}/mongodb.xsl")
    add_dependencies(dbschema dbschema_mongodb)
    add_dependencies(dbschema_clean dbschema_mongodb_clean)
    # Create the version-create.mongo script
    # After processing the JSON files, create the version-create.mongo script
    # Usage of generate_version_create_mongo.sh:
    # 1. The first argument is the path to create the version-create.mongo script
    # 2. The second argument is the path to the directory containing the JSON files
    add_custom_command(
      TARGET dbschema_mongodb
      POST_BUILD
      COMMAND
        bash ${CMAKE_CURRENT_SOURCE_DIR}/generate_version_create_mongo.sh
        "${CMAKE_CURRENT_BINARY_DIR}/mongodb/kamailio/version-create.mongo"
        "${CMAKE_CURRENT_BINARY_DIR}/mongodb/kamailio"
      COMMENT "Creating version-create.mongo from JSON files")

    install(
      FILES ${CMAKE_CURRENT_BINARY_DIR}/mongodb/kamailio/version-create.mongo
      DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/mongodb/${MAIN_NAME}
      COMPONENT KMONGODB)
  endif()
  if("db_redis" IN_LIST FINAL_MODULES_LIST)
    add_db_target(db_redis "${STYLESHEETS}/db_redis.xsl")
    add_dependencies(dbschema dbschema_db_redis)
    add_dependencies(dbschema_clean dbschema_db_redis_clean)
  endif()
endif()
