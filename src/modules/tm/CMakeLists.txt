cmake_minimum_required(VERSION 3.10)

project(tm)

file(GLOB SRC_FILES "*.c")

add_library(tm SHARED ${SRC_FILES})
target_link_libraries(tm PRIVATE common_modules kamailio)

if(${FLAVOUR} STREQUAL "kamailio")
  target_compile_definitions(tm PRIVATE WITH_EVENT_LOCAL_REQUEST)
endif()
