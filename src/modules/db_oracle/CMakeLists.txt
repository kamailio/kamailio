cmake_minimum_required(VERSION 3.10)

project(db_oracle)

file(GLOB SRC_FILES "*.c")

add_library(db_oracle SHARED ${SRC_FILES})
target_link_libraries(db_oracle PRIVATE common_modules kamailio)

target_link_libraries(db_oracle PRIVATE srdb1)
