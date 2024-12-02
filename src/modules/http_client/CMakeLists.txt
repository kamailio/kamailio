cmake_minimum_required(VERSION 3.10)

project(http_client)

find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(http_client SHARED ${SRC_FILES})
target_link_libraries(http_client PRIVATE kamailio CURL::libcurl)
