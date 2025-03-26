file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# libmongoc-1.0 is required for this module Missing dependency on Ubuntu 20.04
# of libzstd-dev find_package(zstd REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(mongoc REQUIRED IMPORTED_TARGET libmongoc-1.0>=1.7)
add_library(mongoc::mongoc ALIAS PkgConfig::mongoc)

target_link_libraries(${module_name} PRIVATE mongoc::mongoc)

include(${CMAKE_SOURCE_DIR}/cmake/dbschema.cmake)
add_db_target("${group_name}" mongodb "${STYLESHEETS}/mongodb.xsl")

# Create the version-create.mongo script
# After processing the JSON files, create the version-create.mongo script
# Usage of generate_version_create_mongo.sh:
# 1. The first argument is the path to create the version-create.mongo script
# 2. The second argument is the path to the directory containing the JSON files
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

install(
  FILES ${CMAKE_BINARY_DIR}/utils/kamctl/mongodb/kamailio/version-create.mongo
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${MAIN_NAME}/mongodb/${MAIN_NAME}
  COMPONENT ${group_name}
)
