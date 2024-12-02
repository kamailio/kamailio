cmake_minimum_required(VERSION 3.10)

project(geoip)

file(GLOB SRC_FILES "*.c")

add_library(geoip SHARED ${SRC_FILES})
target_link_libraries(geoip PRIVATE common_modules kamailio)
