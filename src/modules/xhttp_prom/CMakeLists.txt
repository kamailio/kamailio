cmake_minimum_required(VERSION 3.10)

project(xhttp_prom)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_prom SHARED ${SRC_FILES})
target_link_libraries(xhttp_prom PRIVATE common_modules kamailio)
