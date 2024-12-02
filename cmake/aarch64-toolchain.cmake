set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

find_program(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
find_program(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

if(NOT CMAKE_C_COMPILER)
  message(FATAL_ERROR "aarch64-linux-gnu-g++")
endif()

if(NOT CMAKE_CXX_COMPILER)
  message(FATAL_ERROR "aarch64-linux-gnu-g++")
endif()

# Where to look for the target environment. (More paths can be added here)
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
# set(CMAKE_SYSROOT /usr/aarch64-linux-gnu)

# Adjust the default behavior of the FIND_XXX() commands: search programs in the
# host environment only.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# Search headers and libraries in the target environment only.
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
