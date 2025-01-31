# Quite analogous to the Makefile.defs file This file is used to define the
# common flags and options for the project The flags are defined as INTERFACE
# properties of the common library The flags are then used by the other
# libraries and executables

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
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # Windows specific flags
  include(${OS_SPECIFIC_DIR}/windows.cmake)
elseif()
  message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
endif()
