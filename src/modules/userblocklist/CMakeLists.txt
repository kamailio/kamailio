cmake_minimum_required(VERSION 3.10)

project(userblocklist)

file(GLOB SRC_FILES "*.c")

add_library(userblocklist SHARED ${SRC_FILES})
target_link_libraries(userblocklist PRIVATE common_modules kamailio)

target_link_libraries(userblocklist PRIVATE srdb1)
