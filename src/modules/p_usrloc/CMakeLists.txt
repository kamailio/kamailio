cmake_minimum_required(VERSION 3.10)

project(p_usrloc)

file(GLOB SRC_FILES "*.c")

add_library(p_usrloc SHARED ${SRC_FILES})
target_link_libraries(p_usrloc PRIVATE common_modules kamailio)

target_link_libraries(p_usrloc PRIVATE srdb1)
set_target_properties(
  p_usrloc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
