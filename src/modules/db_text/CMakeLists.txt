file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})
target_compile_definitions(${module_name} PRIVATE DBT_EXTRA_DEBUG)

include(${CMAKE_SOURCE_DIR}/cmake/db_files.cmake)

add_kamctl_db_files(${group_name} kamctl.dbtext)
add_kamdbctl_db_files(${group_name} kamdbctl.dbtext)
install(
  PROGRAMS ${CMAKE_SOURCE_DIR}/utils/kamctl/dbtextdb/dbtextdb.py
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}/kamctl/dbtextdb
  COMPONENT kamailio-core
)
