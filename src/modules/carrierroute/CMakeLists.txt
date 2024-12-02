cmake_minimum_required(VERSION 3.10)

project(carrierroute)

file(GLOB SRC_FILES "*.c")

add_library(carrierroute SHARED ${SRC_FILES})
target_link_libraries(carrierroute PRIVATE common_modules kamailio)

target_link_libraries(carrierroute PRIVATE srdb1)
set_target_properties(
  carrierroute PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
