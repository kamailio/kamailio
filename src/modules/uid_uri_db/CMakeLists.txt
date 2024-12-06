cmake_minimum_required(VERSION 3.10)

project(uid_uri_db)

file(GLOB SRC_FILES "*.c")

add_library(uid_uri_db SHARED ${SRC_FILES})

target_link_libraries(uid_uri_db PRIVATE kamailio)
