cmake_minimum_required(VERSION 3.10)

project(xcap_client)

find_package(CURL REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(xcap_client SHARED ${SRC_FILES})

target_link_libraries(xcap_client PRIVATE kamailio CURL::libcurl)
target_link_libraries(xcap_client PRIVATE srdb1)
