cmake_minimum_required(VERSION 3.10)

project(db_berkeley)

# libdb++-dev
find_package(BerkeleyDB REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(db_berkeley SHARED ${SRC_FILES})
target_link_libraries(db_berkeley PRIVATE kamailio BerkeleyDB::BerkeleyDB)
