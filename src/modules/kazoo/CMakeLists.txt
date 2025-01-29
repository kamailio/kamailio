file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# TODO: Create its own FindJsonC.cmake module since it;s used by multiple
# modules throughout the project
# TODO: When we have a proper cmake config file for json-c for most OS, we can
# use that instead of pkg-config.
# json-c v 0.14 and 0.15 have wrong include directories in their cmake config
# files. uncomment the following line when we have a proper cmake config file
# for json-c for most OS.
# find_package(json-c 0.16 CONFIG)

# if(NOT json-c_FOUND)
# message(STATUS "json-c not found. looking with pkg-config")
find_package(PkgConfig REQUIRED)
pkg_check_modules(json-c REQUIRED IMPORTED_TARGET json-c)
add_library(json-c::json-c ALIAS PkgConfig::json-c)
# endif()

find_package(rabbitmq-c 0.13)
if(NOT rabbitmq-c_FOUND)
  message(STATUS "rabbitmq-c not found. looking with pkg-config")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(rabbitmq-c REQUIRED IMPORTED_TARGET librabbitmq)
  add_library(rabbitmq::rabbitmq ALIAS PkgConfig::rabbitmq-c)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

target_link_libraries(${module_name} PRIVATE json-c::json-c rabbitmq::rabbitmq
                                             uuid::uuid)
