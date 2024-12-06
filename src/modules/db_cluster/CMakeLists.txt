cmake_minimum_required(VERSION 3.10)

project(db_cluster)

file(GLOB SRC_FILES "*.c")

add_library(db_cluster SHARED ${SRC_FILES})

target_link_libraries(db_cluster PRIVATE kamailio)
