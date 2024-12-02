cmake_minimum_required(VERSION 3.10)

project(ims_usrloc_scscf)

file(GLOB SRC_FILES "*.c")

add_library(ims_usrloc_scscf SHARED ${SRC_FILES})
target_link_libraries(ims_usrloc_scscf PRIVATE common_modules kamailio)

target_link_libraries(ims_usrloc_scscf PRIVATE srdb1)
set_target_properties(
  ims_usrloc_scscf PROPERTIES INSTALL_RPATH
                              "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
