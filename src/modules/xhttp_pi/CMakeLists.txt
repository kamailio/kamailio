cmake_minimum_required(VERSION 3.10)

project(xhttp_pi)

file(GLOB SRC_FILES "*.c")

add_library(xhttp_pi SHARED ${SRC_FILES})
target_link_libraries(xhttp_pi PRIVATE common_modules kamailio)

target_link_libraries(xhttp_pi PRIVATE srdb1)
