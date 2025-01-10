find_package(BerkeleyDB REQUIRED)

add_executable(kamdb_recover EXCLUDE_FROM_ALL)

target_sources(kamdb_recover PRIVATE kambdb_recover.c)

target_link_libraries(kamdb_recover PRIVATE BerkeleyDB::BerkeleyDB)

install(
  TARGETS kamdb_recover
  DESTINATION ${CMAKE_INSTALL_SBINDIR}
  OPTIONAL)
