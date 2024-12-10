file(GLOB SRC_FILES "*.c")

target_sources(kamailio PUBLIC ${SRC_FILES})

# ----------
# dbschema target:
# Old Makefiles: make dbschema declared in src/ Makefile
# This was using the makefile found in src/lib/srdb1/schema folder.
#
# TODO: Revise where we want the target declared.
# New CMakelists.txt
# Declare the target in srdb1 CMakeLists.txt file.

find_program(XSLTPROC_EXECUTABLE xsltproc REQUIRED)

option(XSLT_VALIDATE, "Enable schema validation during XSL transformations" ON)
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
set(ROOTDOC ${CMAKE_SOURCE_DIR}/src)
set(STYLESHEETS ${ROOTDIR}/doc/stylesheets/dbschema_k/xsl)
set(SCHEME ${ROOTDIR}/utils/kamctl)

set(CATALOG ${ROOTDIR}/doc/stylesheets/dbschema_k/catalog.xml)
# List of XML files
file(GLOB TABLES "${CMAKE_CURRENT_SOURCE_DIR}/schema/kamailio-*.xml")
message(WARNING "TABLES : ${TABLES}")
set(EXTRACTED_TABLES "")
foreach(TABLE ${TABLES})
  get_filename_component(TABLE_NAME "${TABLE}" NAME)
  string(REPLACE "kamailio-" "" TABLE_NAME "${TABLE_NAME}")
  string(REPLACE ".xml" "" TABLE_NAME "${TABLE_NAME}")
  list(APPEND EXTRACTED_TABLES "${TABLE_NAME}")
endforeach()
# Output the extracted table names
message(STATUS "Extracted Tables: ${EXTRACTED_TABLES}")
# Function to add a target for each database type
function(add_db_target db_name xsl_file)
  add_custom_target(
    dbschema_${db_name} COMMAND ${CMAKE_COMMAND} -E make_directory
                                "${SCHEME}/${db_name}"
  )
  #         COMMAND
  #       ${XSLTPROC_EXECUTABLE} --xinclude --stringparam dir "${SCHEME}/${db_name}"
  #       --stringparam db "${db_name}" "${xsl_file}" ${TABLES}
  #     DEPENDS ${TABLES}
  #     WORKING_DIRECTORY "${SCHEME}/${db_name}"
  #   )

  # Loop through each table and add a command for xsltproc
  foreach(TABLE ${EXTRACTED_TABLES})
    add_custom_command(
      TARGET dbschema_${db_name}
      COMMAND
        ${XSLTPROC_EXECUTABLE} ${XSLTPROC_FLAGS} --stringparam dir
        "${SCHEME}/${db_name}" --stringparam prefix "${TABLE}-" --stringparam db
        "${db_name}" "${xsl_file}"
        "${CMAKE_CURRENT_SOURCE_DIR}/schema/kamailio-${TABLE}.xml"
      WORKING_DIRECTORY "${SCHEME}/${db_name}"
      COMMENT "Processing ${TABLE} for ${db_name}"
    )
  endforeach()

  add_custom_target(
    dbschema_${db_name}_clean COMMAND ${CMAKE_COMMAND} -E remove_directory
                                      "${SCHEME}/${db_name}"
  )
endfunction()

# Add targets for each database type
# Pi framework targets should combined into one
# add_db_target(pi_framework_table "${STYLESHEETS}/pi_framework_table.xsl")
# add_db_target(pi_framework_mod "${STYLESHEETS}/pi_framework_mod.xsl")
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
)
