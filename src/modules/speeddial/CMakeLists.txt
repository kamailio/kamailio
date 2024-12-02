cmake_minimum_required(VERSION 3.10)

project(speeddial)

file(GLOB SRC_FILES "*.c")

add_library(speeddial SHARED ${SRC_FILES})
target_link_libraries(speeddial PRIVATE common common_modules kamailio)

# TODO: link with srdb1