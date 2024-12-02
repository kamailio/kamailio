cmake_minimum_required(VERSION 3.10)

project(ims_ipsec_pcscf)

file(GLOB SRC_FILES "*.c")

add_library(ims_ipsec_pcscf SHARED ${SRC_FILES})
target_link_libraries(ims_ipsec_pcscf PRIVATE common_modules kamailio)
