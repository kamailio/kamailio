cmake_minimum_required(VERSION 3.10)

project(evapi)

find_package(Libev REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(evapi SHARED ${SRC_FILES})
target_link_libraries(evapi PRIVATE kamailio Libev::Libev)
