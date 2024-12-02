cmake_minimum_required(VERSION 3.10)

project(cfg_db)

file(GLOB SRC_FILES "*.c")

add_library(cfg_db SHARED ${SRC_FILES})

target_link_libraries(cfg_db PRIVATE kamailio)
target_link_libraries(cfg_db PRIVATE srdb2)
