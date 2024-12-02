cmake_minimum_required(VERSION 3.10)

project(kamailio_lib_srdb1 VERSION 1.0.0)

file(GLOB SRC_FILES "*.c")

add_library(srdb1 SHARED ${SRC_FILES})
target_link_libraries(srdb1 common kamailio)
set_target_properties(srdb1 PROPERTIES VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
set_target_properties(srdb1 PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
