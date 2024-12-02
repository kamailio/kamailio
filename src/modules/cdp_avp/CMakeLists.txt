cmake_minimum_required(VERSION 3.10)

project(cdp_avp)

file(GLOB SRC_FILES "*.c")

add_library(cdp_avp SHARED ${SRC_FILES})
target_link_libraries(cdp_avp PRIVATE kamailio)
