cmake_minimum_required(VERSION 3.10)

project(ldap)

file(GLOB SRC_FILES "*.c")

add_library(ldap SHARED ${SRC_FILES})
target_link_libraries(ldap PRIVATE common_modules kamailio)
