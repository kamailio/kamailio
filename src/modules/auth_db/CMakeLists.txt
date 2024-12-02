cmake_minimum_required(VERSION 3.10)

project(auth_db)

file(GLOB SRC_FILES "*.c")

add_library(auth_db SHARED ${SRC_FILES})
target_link_libraries(auth_db PRIVATE kamailio)

target_link_libraries(auth_db PRIVATE srdb1)
