cmake_minimum_required(VERSION 3.10)

project(snmpstats)

file(GLOB SRC_FILES "*.c")

add_library(snmpstats SHARED ${SRC_FILES})
target_link_libraries(snmpstats PRIVATE common_modules kamailio)
