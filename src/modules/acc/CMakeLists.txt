cmake_minimum_required(VERSION 3.10)

project(acc)

file(GLOB acc_SRC "*.c")

add_library(acc SHARED ${acc_SRC})
target_link_libraries(acc PRIVATE common_modules kamailio)

target_link_libraries(acc PRIVATE srdb1)
