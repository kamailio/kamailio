cmake_minimum_required(VERSION 3.10)

project(ndb_cassandra)

file(GLOB SRC_FILES "*.c")

add_library(ndb_cassandra SHARED ${SRC_FILES})
target_link_libraries(ndb_cassandra PRIVATE common_modules kamailio)
