cmake_minimum_required(VERSION 3.10)

project(rabbitmq)

file(GLOB SRC_FILES "*.c")

add_library(rabbitmq SHARED ${SRC_FILES})
target_link_libraries(rabbitmq PRIVATE common_modules kamailio)
