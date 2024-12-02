cmake_minimum_required(VERSION 3.10)

project(uid_avp_db)

file(GLOB SRC_FILES "*.c")

add_library(uid_avp_db SHARED ${SRC_FILES})
target_link_libraries(uid_avp_db PRIVATE common_modules kamailio)
