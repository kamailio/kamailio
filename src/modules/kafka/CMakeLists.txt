file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(RdKafka QUIET)
if(NOT RdKafka_FOUND)
  message(STATUS "RdKafka not found... checking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(RDKAFKA REQUIRED IMPORTED_TARGET rdkafka)
  add_library(rdkafka++ ALIAS PkgConfig::RDKAFKA)
endif()

target_link_libraries(${module_name} PRIVATE rdkafka++)
