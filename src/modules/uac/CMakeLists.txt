cmake_minimum_required(VERSION 3.10)

project(uac)

file(GLOB SRC_FILES "*.c")

add_library(uac SHARED ${SRC_FILES})
target_link_libraries(uac PRIVATE common common_modules kamailio)

# TODO: link with srdb1