file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: Needs some testing for OS that support OracleDB
find_package(Oracle REQUIRED)

# /usr/include/oracle/18.3/client64)
target_include_directories(${module_name} PRIVATE ${ORACLE_INCLUDE_DIR})
target_link_libraries(${module_name} PRIVATE ${ORACLE_LIBRARIES})

include(${CMAKE_SOURCE_DIR}/cmake/db_files.cmake)
add_kamctl_db_files(${group_name} kamctl.oracle)
add_kamdbctl_db_files(${group_name} kamdbctl.oracle kamdbfunc.oracle)

include(${CMAKE_SOURCE_DIR}/cmake/dbschema.cmake)
add_db_target("${group_name}" db_oracle "${STYLESHEETS}/oracle.xsl")
