cmake_minimum_required(VERSION 3.10)

project(imc)

file(GLOB SRC_FILES "*.c")

add_library(imc SHARED ${SRC_FILES})
target_link_libraries(imc PRIVATE common_modules kamailio)

target_link_libraries(imc PRIVATE srdb1)
set_target_properties(
  imc PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
