cmake_minimum_required(VERSION 3.10)

project(kamailio_utils)

# if(NOT basedir) set(KAMCTL_TMP_DIR "/tmp") else() set(KAMCTL_TMP_DIR
# "${basedir}") endif()

set(KAMCTL_TMP_DIR "${CMAKE_BINARY_DIR}/kamctltmp")
message(STATUS "Temporary kamctl.{db} files in : ${KAMCTL_TMP_DIR}")

file(MAKE_DIRECTORY ${KAMCTL_TMP_DIR})

set(BIN_DIR "${CMAKE_INSTALL_FULL_SBINDIR}")
set(LIB_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/${MAIN_NAME}")
set(CFG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${MAIN_NAME}")
set(SHARE_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${MAIN_NAME}")

set(KAMCTL_BASE_FILES
    kamctl.base
    kamctl.ctlbase
    kamctl.db_berkeley
    kamctl.dbtext
    kamctl.mysql
    kamctl.oracle
    kamctl.pgsql
    kamctl.rpcfifo
    kamctl.ser
    kamctl.sqlbase
    kamctl.sqlite)

# Install kamctl and it's database files
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamctl"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamctl > ${KAMCTL_TMP_DIR}/kamctl
  COMMENT "Processed kamctl with sed")
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin)

# Create the depependencies list for the target kamctl
set(KAMCTL_DEPENDENCIES "${KAMCTL_TMP_DIR}/kamctl")

# Install kamctl.base and the rest
foreach(FILE ${KAMCTL_BASE_FILES})
  # Append to the depependencies list for the target kamctl
  list(APPEND KAMCTL_DEPENDENCIES "${KAMCTL_TMP_DIR}/${FILE}")
  add_custom_command(
    OUTPUT "${KAMCTL_TMP_DIR}/${FILE}"
    COMMAND sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
            ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} > ${KAMCTL_TMP_DIR}/${FILE}
    COMMENT "Processed ${FILE} with sed")
  install(
    PROGRAMS ${KAMCTL_TMP_DIR}/${FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

# Kamctl target to depend on all kamctl files
add_custom_target(kamctl ALL DEPENDS ${KAMCTL_DEPENDENCIES})

# Setup and install kamdbctl stuff
set(KAMDB_BASE_FILES
    kamdbctl.base
    kamdbctl.db_berkeley
    kamdbctl.dbtext
    kamdbctl.mysql
    kamdbctl.oracle
    kamdbctl.pgsql
    kamdbctl.sqlite
    kamdbfunc.oracle)

# Install kamdbctl
add_custom_command(
  OUTPUT "${KAMCTL_TMP_DIR}/kamdbctl"
  COMMAND
    sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
    "s#/usr/local/lib/kamailio#${LIB_DIR}#g" -e
    "s#/usr/local/etc/kamailio#${CFG_DIR}#g" <
    ${CMAKE_CURRENT_SOURCE_DIR}/kamdbctl > ${KAMCTL_TMP_DIR}/kamdbctl
  COMMENT "Processed kamdbctl with sed")
install(
  PROGRAMS ${KAMCTL_TMP_DIR}/kamdbctl
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  COMPONENT kamailio-utils-bin)

set(KAMDB_DEPENDENCIES "${KAMCTL_TMP_DIR}/kamdbctl")

foreach(FILE ${KAMDB_BASE_FILES})
  list(APPEND KAMDB_DEPENDENCIES "${KAMCTL_TMP_DIR}/${FILE}")
  # Install db setup base script
  add_custom_command(
    OUTPUT "${KAMCTL_TMP_DIR}/${FILE}"
    COMMAND
      sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
      "s#/usr/local/etc/kamailio#${CFG_DIR}#g" -e
      "s#/usr/local/share/kamailio#${SHARE_DIR}#g" <
      ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} > ${KAMCTL_TMP_DIR}/${FILE}
    COMMENT "Processed ${FILE}")
  install(
    PROGRAMS ${KAMCTL_TMP_DIR}/${FILE}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

add_custom_target(kamdbctl ALL DEPENDS ${KAMDB_DEPENDENCIES})

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
  COMPONENT kamailio-utils-cfg)

# Custom target for installing the kamailio-cfg component
add_custom_target(
  install-kamailio-utils-cfg
  COMMAND ${CMAKE_COMMAND} --install . --component kamailio-utils-cfg
  COMMENT "Installing kamailio-utils-cfg component")

# Custom target for installing the kamailio-utils-bin component
add_custom_target(
  install-kamailio-utils-bin
  COMMAND ${CMAKE_COMMAND} --install . --component kamailio-utils-bin
  COMMENT "Installing kamailio-utils-bin component"
  DEPENDS kamctl kamdbctl)

# Custom target for installing the kamailio-utils component
add_custom_target(
  install-kamailio-utils # COMMENT "Installing kamailio-utils-cfg and
                         # kamailio-utils-bin components"
  DEPENDS install-kamailio-utils-cfg install-kamailio-utils-bin)

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
  foreach(TABLE ${TABLES})
    get_filename_component(TABLE_NAME "${TABLE}" NAME)
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
      set(db_name_folder xhttpi_pi)
    else()
      set(db_name_folder ${db_name})
    endif()

    add_custom_target(
      dbschema_${db_name}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${db_name_folder}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

    # Loop through each table and add a command for xsltproc
    foreach(TABLE ${EXTRACTED_TABLES})
      # Determine the prefix based on db_name
      if(db_name STREQUAL "db_berkeley"
         OR db_name STREQUAL "db_redis"
         OR db_name STREQUAL "dbtext"
         OR db_name STREQUAL "mongodb")
        set(PREFIX '')
      else()
        set(PREFIX "${TABLE}-")
      endif()

      add_custom_command(
        TARGET dbschema_${db_name}
        PRE_BUILD
        COMMAND
          XML_CATALOG_FILES=${CATALOG} ${XSLTPROC_EXECUTABLE} ${XSLTPROC_FLAGS}
          --stringparam dir ${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}
          --stringparam prefix ${PREFIX} --stringparam db ${db_name} ${xsl_file}
          "kamailio-${TABLE}.xml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/lib/srdb1/schema"
        COMMENT "Processing ${TABLE} for ${db_name}")
    endforeach()

    add_custom_target(
      dbschema_${db_name}_clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory
              "${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}")
  endfunction()

  # Add targets for each database type Pi framework targets should combined into
  # one
  add_db_target(pi_framework_table "${STYLESHEETS}/pi_framework_table.xsl")
  add_db_target(pi_framework_mod "${STYLESHEETS}/pi_framework_mod.xsl")
  add_db_target(mysql "${STYLESHEETS}/mysql.xsl")
  add_db_target(postgres "${STYLESHEETS}/postgres.xsl")
  add_db_target(oracle "${STYLESHEETS}/oracle.xsl")
  add_db_target(dbtext "${STYLESHEETS}/dbtext.xsl")
  add_db_target(db_berkeley "${STYLESHEETS}/db_berkeley.xsl")
  add_db_target(db_sqlite "${STYLESHEETS}/db_sqlite.xsl")
  add_db_target(mongodb "${STYLESHEETS}/mongodb.xsl")
  add_db_target(db_redis "${STYLESHEETS}/db_redis.xsl")

  add_custom_target(
    dbschema
    DEPENDS dbschema_mysql
            dbschema_postgres
            dbschema_oracle
            dbschema_dbtext
            dbschema_db_berkeley
            dbschema_db_sqlite
            dbschema_mongodb
            dbschema_db_redis
            dbschema_pi_framework_mod
            dbschema_pi_framework_table)

  add_custom_target(
    dbschema_clean
    DEPENDS dbschema_mysql_clean
            dbschema_postgres_clean
            dbschema_oracle_clean
            dbschema_dbtext_clean
            dbschema_db_berkeley_clean
            dbschema_db_sqlite_clean
            dbschema_mongodb_clean
            dbschema_db_redis_clean
            dbschema_pi_framework_mod_clean
            dbschema_pi_framework_table_clean)
endif()
