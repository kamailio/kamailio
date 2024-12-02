cmake_minimum_required(VERSION 3.10)

project(ims_icscf)

file(GLOB SRC_FILES "*.c")

add_library(ims_icscf SHARED ${SRC_FILES})
target_link_libraries(ims_icscf PRIVATE common_modules kamailio)

target_link_libraries(ims_icscf PRIVATE srdb1)
set_target_properties(
  ims_icscf PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
