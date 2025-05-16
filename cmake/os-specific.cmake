# This file is included from the def.cmake CMakeLists.txt file.
# It sets up the OS-specific flags and includes the appropriate
# OS-specific CMake file.
set(OS_SPECIFIC_DIR "${CMAKE_SOURCE_DIR}/cmake/os-specific")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  include(${OS_SPECIFIC_DIR}/linux.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "DragonFly")
  # DragonFly BSD specific flags
  include(${OS_SPECIFIC_DIR}/dragonfly.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  # Mac OS X specific flags
  include(${OS_SPECIFIC_DIR}/darwin.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  # FreeBSD specific flags
  include(${OS_SPECIFIC_DIR}/freebsd.cmake)
elseif()
  message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
endif()
