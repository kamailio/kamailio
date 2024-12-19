# TODO: Create its own FindJsonC.cmake module since it;s used by multiple
# modules throughout the project
find_package(json-c QUIET)
if(NOT json-c_FOUND)
  message(STATUS "json-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
  add_library(json-c::json-c ALIAS PkgConfig::json-c)
endif()

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

add_library(kazoo SHARED ${SRC_FILES})

target_link_libraries(kazoo PRIVATE json-c::json-c rabbitmq::rabbitmq
                                    uuid::uuid)
