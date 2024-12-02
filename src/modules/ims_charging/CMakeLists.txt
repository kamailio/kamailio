cmake_minimum_required(VERSION 3.10)

project(ims_charging)

file(GLOB SRC_FILES "*.c")

add_library(ims_charging SHARED ${SRC_FILES})
target_link_libraries(ims_charging PRIVATE common_modules kamailio)

target_link_libraries(ims_charging PRIVATE srdb1)
set_target_properties(
  ims_charging PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
