cmake_minimum_required(VERSION 3.10)

project(geoip2)

file(GLOB SRC_FILES "*.c")

add_library(geoip2 SHARED ${SRC_FILES})
target_link_libraries(geoip2 PRIVATE common_modules kamailio)
