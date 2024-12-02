cmake_minimum_required(VERSION 3.10)

project(acc)

file(GLOB acc_SRC
    "*.c"
)

# Define the library target
add_library(acc SHARED ${acc_SRC})
target_link_libraries(acc PRIVATE common common_modules kamailio)
target_link_libraries(acc PRIVATE srdb1)
set_target_properties(acc PROPERTIES
  INSTALL_RPATH "${CMAKE_INSTALL_LIBDIR}/${MAIN_NAME}"
)