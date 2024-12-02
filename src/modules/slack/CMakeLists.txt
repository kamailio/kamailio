cmake_minimum_required(VERSION 3.10)

project(slack)

find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(slack SHARED ${SRC_FILES})
target_link_libraries(slack PRIVATE kamailio CURL::libcurl)
