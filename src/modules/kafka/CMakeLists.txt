file(GLOB SRC_FILES "*.c")

find_package(RdKafka QUIET)
if(NOT RdKafka_FOUND)
  message(STATUS "RdKafka not found... checking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka)
  add_library(rdkafka++ ALIAS PkgConfig::RDKAFKA)
endif()

add_library(kafka SHARED ${SRC_FILES})
target_link_libraries(kafka PRIVATE rdkafka++)
