file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: find include file required
find_path(
  SCTP_INCLUDE_DIR
  NAMES sctp.h
  PATHS /usr/include /usr/local/include /usr/include/netinet)

if(NOT SCTP_INCLUDE_DIR)
  message(FATAL_ERROR "sctp.h not found. maybe missing libsctp-dev?")
endif()

target_include_directories(${module_name} PRIVATE ${SCTP_INCLUDE_DIR})

find_library(
  SCTP_LIBRARY
  NAMES sctp
  PATHS /usr/lib /usr/local/lib)

if(UNIX)
  target_link_libraries(${module_name} PRIVATE ${SCTP_LIBRARY})
endif()
