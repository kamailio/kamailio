file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# libdb++-dev
find_package(BerkeleyDB REQUIRED)

target_link_libraries(${module_name} PRIVATE BerkeleyDB::BerkeleyDB)

# Build also the util function found in utils/db_berkeley/
# This is needed to build the kamdb_recover utility
add_subdirectory(${CMAKE_SOURCE_DIR}/utils/db_berkeley ${CMAKE_BINARY_DIR}/utils/db_berkeley)
add_dependencies(${module_name} kamdb_recover)

include(${CMAKE_SOURCE_DIR}/cmake/db_files.cmake)
add_kamctl_db_files(${group_name} kamctl.db_berkeley)
add_kamdbctl_db_files(${group_name} kamdbctl.db_berkeley)
