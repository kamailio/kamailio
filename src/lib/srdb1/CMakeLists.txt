cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_srdb1 VERSION 1.0.0)

file(GLOB SRC_FILES "*.c")

add_library(lib_srdb1 SHARED ${SRC_FILES})
target_link_libraries(lib_srdb1 common kamailio)

# TODO: do we need version control?
set_target_properties(
  lib_srdb1 PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                       OUTPUT_NAME srdb1
)
set_target_properties(lib_srdb1 PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

add_library(srdb1 ALIAS lib_srdb1)
