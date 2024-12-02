cmake_minimum_required(VERSION 3.10)

project(rabbitmq)

find_package(rabbitmq-c 0.13 QUIET)
if(NOT rabbitmq-c_FOUND)
  message(STATUS "rabbitmq-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(rabbitmq-c REQUIRED IMPORTED_TARGET librabbitmq)
  add_library(rabbitmq::rabbitmq ALIAS PkgConfig::rabbitmq-c)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

file(GLOB SRC_FILES "*.c")

add_library(rabbitmq SHARED ${SRC_FILES})

if(rabbitmq-c_VERSION VERSION_GREATER_EQUAL "0.13")
  target_compile_definitions(rabbitmq PRIVATE RABBITMQ_DEPRECATION)
endif()

target_link_libraries(rabbitmq PRIVATE kamailio rabbitmq::rabbitmq uuid::uuid)
