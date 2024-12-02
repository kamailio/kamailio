cmake_minimum_required(VERSION 3.10)

project(ims_registrar_scscf)

file(GLOB SRC_FILES "*.c")

add_library(ims_registrar_scscf SHARED ${SRC_FILES})
target_link_libraries(ims_registrar_scscf PRIVATE common_modules kamailio)
