cmake_minimum_required(VERSION 3.10)

project(sctp)

file(GLOB SRC_FILES "*.c")

add_library(sctp SHARED ${SRC_FILES})
if(UNIX)
  target_link_libraries(sctp PRIVATE -lsctp)
endif()
target_link_libraries(sctp PRIVATE kamailio)
