cmake_minimum_required(VERSION 3.10)

project(userblocklist)

file(GLOB SRC_FILES "*.c")

add_library(userblocklist SHARED ${SRC_FILES})
target_link_libraries(userblocklist PRIVATE common_modules kamailio)

target_link_libraries(userblocklist PRIVATE srdb1)
set_target_properties(
  userblocklist PROPERTIES INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)
