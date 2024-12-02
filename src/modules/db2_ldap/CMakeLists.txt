cmake_minimum_required(VERSION 3.10)

project(db2_ldap)

file(GLOB SRC_FILES "*.c")

add_library(db2_ldap SHARED ${SRC_FILES})
target_link_libraries(db2_ldap PRIVATE common_modules kamailio)
