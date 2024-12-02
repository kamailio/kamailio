cmake_minimum_required(VERSION 3.10)

project(ndb_mongodb)

file(GLOB SRC_FILES "*.c")

add_library(ndb_mongodb SHARED ${SRC_FILES})
target_link_libraries(ndb_mongodb PRIVATE common_modules kamailio)
