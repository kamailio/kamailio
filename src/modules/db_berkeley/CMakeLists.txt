file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# libdb++-dev
find_package(BerkeleyDB REQUIRED)

target_link_libraries(${module_name} PRIVATE BerkeleyDB::BerkeleyDB)

# Build also the util function found in utils/db_berkeley/
add_dependencies(${module_name} kamdb_recover)
