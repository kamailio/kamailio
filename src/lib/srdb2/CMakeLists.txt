cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_srdb2 VERSION 1.0.0)

file(GLOB SRC_FILES "*.c")

add_library(lib_srdb2 SHARED ${SRC_FILES})
target_link_libraries(lib_srdb2 common kamailio)

set_target_properties(
  lib_srdb2 PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                       OUTPUT_NAME srdb2
)
set_target_properties(lib_srdb2 PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
add_library(srdb2 ALIAS lib_srdb2)
