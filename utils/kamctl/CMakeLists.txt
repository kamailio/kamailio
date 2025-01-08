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
foreach(file ${KAMCTL_BASE_FILES})
  # Append to the depependencies list for the target kamctl
  list(APPEND KAMCTL_DEPENDENCIES "${KAMCTL_TMP_DIR}/${file}")
  add_custom_command(
    OUTPUT "${KAMCTL_TMP_DIR}/${file}"
    COMMAND sed -e "s#/usr/local/sbin#${BIN_DIR}#g" <
            ${CMAKE_CURRENT_SOURCE_DIR}/${file} > ${KAMCTL_TMP_DIR}/${file}
    COMMENT "Processed ${file} with sed")
  install(
    PROGRAMS ${KAMCTL_TMP_DIR}/${file}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

# Kamctl target to depend on all kamctl files
add_custom_target(
  kamctl ALL
  DEPENDS ${KAMCTL_DEPENDENCIES}
  COMMENT "Build kamctl and its dependencies")

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

foreach(file ${KAMDB_BASE_FILES})
  list(APPEND KAMDB_DEPENDENCIES "${KAMCTL_TMP_DIR}/${file}")
  # Install db setup base script
  add_custom_command(
    OUTPUT "${KAMCTL_TMP_DIR}/${file}"
    COMMAND
      sed -e "s#/usr/local/sbin#${BIN_DIR}#g" -e
      "s#/usr/local/etc/kamailio#${CFG_DIR}#g" -e
      "s#/usr/local/share/kamailio#${SHARE_DIR}#g" <
      ${CMAKE_CURRENT_SOURCE_DIR}/${file} > ${KAMCTL_TMP_DIR}/${file}
    COMMENT "Processed ${file}")
  install(
    PROGRAMS ${KAMCTL_TMP_DIR}/${file}
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl
    COMPONENT kamailio-utils-bin)
endforeach()

add_custom_target(
  kamdbctl ALL
  DEPENDS ${KAMDB_DEPENDENCIES}
  COMMENT "Build kamdbctl and its dependencies")

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
  FILES ${CMAKE_CURRENT_BINARY_DIR}/kamctl.8 ${CMAKE_CURRENT_BINARY_DIR}/kamdbctl.8
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
      set(db_name_folder xhttpi_pi)
    else()
      set(db_name_folder ${db_name})
    endif()

    add_custom_target(
      dbschema_${db_name}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${db_name_folder}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
      COMMENT "Creating scehmas for ${db_name}")

    # Loop through each table and add a command for xsltproc
    foreach(table ${EXTRACTED_TABLES})
      # Determine the prefix based on db_name
      if(db_name STREQUAL "db_berkeley"
         OR db_name STREQUAL "db_redis"
         OR db_name STREQUAL "dbtext"
         OR db_name STREQUAL "mongodb")
        set(prefix '')
      else()
        set(prefix "${table}-")
      endif()

      add_custom_command(
        TARGET dbschema_${db_name}
        PRE_BUILD
        COMMAND
          XML_CATALOG_FILES=${CATALOG} ${XSLTPROC_EXECUTABLE} ${XSLTPROC_FLAGS}
          --stringparam dir ${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}
          --stringparam prefix ${prefix} --stringparam db ${db_name} ${xsl_file}
          "kamailio-${table}.xml"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/src/lib/srdb1/schema"
        COMMENT "Processing ${table} for ${db_name}")
    endforeach()

    add_custom_target(
      dbschema_${db_name}_clean
      COMMAND ${CMAKE_COMMAND} -E remove_directory
              "${CMAKE_CURRENT_BINARY_DIR}/${db_name_folder}"
      COMMENT "Cleaning ${db_name} schema files")
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
            dbschema_pi_framework_table
    COMMENT "Generating schemas for all dbs...")

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
            dbschema_pi_framework_table_clean
    COMMENT "Cleaning schemas for all dbs...")
endif()
