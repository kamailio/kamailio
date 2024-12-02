cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_print VERSION 1.2.1)

file(GLOB SRC_FILES "*.c")

# Original target to be named as folder, so when included from lib/CMakeLists.txt
# it will be named as lib/ims.so
add_library(lib_print SHARED ${SRC_FILES})

target_link_libraries(lib_print common kamailio)

# TODO: do we need version control?
set_target_properties(
  lib_print PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
                       OUTPUT_NAME print
)
set_target_properties(lib_print PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
