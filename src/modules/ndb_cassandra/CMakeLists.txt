find_package(PkgConfig REQUIRED)
pkg_check_modules(thrift REQUIRED IMPORTED_TARGET thrift)
add_library(thrift::thrift ALIAS PkgConfig::thrift)

file(GLOB SRC_FILES "*.c")

add_library(ndb_cassandra SHARED ${SRC_FILES})
target_link_libraries(ndb_cassandra PRIVATE thrift::thrift)
