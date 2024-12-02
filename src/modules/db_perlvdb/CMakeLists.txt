cmake_minimum_required(VERSION 3.10)

project(db_perlvdb)

file(GLOB SRC_FILES "*.c")

add_library(db_perlvdb SHARED ${SRC_FILES})
target_link_libraries(db_perlvdb PRIVATE common_modules kamailio)

target_link_libraries(db_perlvdb PRIVATE srdb1)
