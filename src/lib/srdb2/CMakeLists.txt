cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_srdb2 VERSION 1.0.0)

file(GLOB SRC_FILES "*.c")

add_library(srdb2 SHARED ${SRC_FILES})
target_link_libraries(srdb2 common kamailio)

set_target_properties(
  srdb2 PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)
set_target_properties(srdb2 PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
