cmake_minimum_required(VERSION 3.10)

project(db_sqlite)

file(GLOB SRC_FILES "*.c")

find_package(SQLite3 REQUIRED)

add_library(db_sqlite SHARED ${SRC_FILES})
target_link_libraries(db_sqlite PRIVATE kamailio SQLite::SQLite3)
