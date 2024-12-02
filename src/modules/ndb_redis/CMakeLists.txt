cmake_minimum_required(VERSION 3.10)

project(ndb_redis)

file(GLOB SRC_FILES "*.c")

add_library(ndb_redis SHARED ${SRC_FILES})
target_link_libraries(ndb_redis PRIVATE common_modules kamailio)
