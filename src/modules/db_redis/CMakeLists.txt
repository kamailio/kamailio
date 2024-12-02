cmake_minimum_required(VERSION 3.10)

project(db_redis)

file(GLOB SRC_FILES "*.c")

add_library(db_redis SHARED ${SRC_FILES})
target_link_libraries(db_redis PRIVATE common_modules kamailio)

target_link_libraries(db_redis PRIVATE srdb1)
