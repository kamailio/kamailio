cmake_minimum_required(VERSION 3.10)

project(domainpolicy)

file(GLOB SRC_FILES "*.c")

add_library(domainpolicy SHARED ${SRC_FILES})
target_link_libraries(domainpolicy PRIVATE common_modules kamailio)

target_link_libraries(domainpolicy PRIVATE srdb1)
