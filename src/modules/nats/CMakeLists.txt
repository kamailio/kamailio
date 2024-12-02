cmake_minimum_required(VERSION 3.10)

project(nats)

file(GLOB SRC_FILES "*.c")

add_library(nats SHARED ${SRC_FILES})
target_link_libraries(nats PRIVATE common_modules kamailio)
