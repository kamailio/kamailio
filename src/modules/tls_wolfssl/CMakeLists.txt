cmake_minimum_required(VERSION 3.10)

project(tls_wolfssl)

file(GLOB SRC_FILES "*.c")

add_library(tls_wolfssl SHARED ${SRC_FILES})
target_link_libraries(tls_wolfssl PRIVATE common_modules kamailio)
