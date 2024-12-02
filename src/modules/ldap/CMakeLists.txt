cmake_minimum_required(VERSION 3.10)

project(ldap)

file(GLOB SRC_FILES "*.c")

find_package(Ldap REQUIRED)

add_library(ldap SHARED ${SRC_FILES})
target_link_libraries(ldap PRIVATE kamailio Ldap::Ldap)
