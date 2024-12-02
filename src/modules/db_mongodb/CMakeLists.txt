cmake_minimum_required(VERSION 3.10)

project(db_mongodb)

file(GLOB SRC_FILES "*.c")

add_library(db_mongodb SHARED ${SRC_FILES})
target_link_libraries(db_mongodb PRIVATE common_modules kamailio)

target_link_libraries(db_mongodb PRIVATE srdb1)
