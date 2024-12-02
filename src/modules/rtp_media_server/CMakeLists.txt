cmake_minimum_required(VERSION 3.10)

project(rtp_media_server)

file(GLOB SRC_FILES "*.c")

add_library(rtp_media_server SHARED ${SRC_FILES})
target_link_libraries(rtp_media_server PRIVATE common_modules kamailio)
