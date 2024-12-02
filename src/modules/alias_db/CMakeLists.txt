cmake_minimum_required(VERSION 3.10)

project(alias_db)

file(GLOB SRC_FILES "*.c")

add_library(alias_db SHARED ${SRC_FILES})
target_link_libraries(alias_db PRIVATE common common_modules kamailio common_modules kamailio common_modules kamailio common_modules kamailio)

# TODO: link with srdb1