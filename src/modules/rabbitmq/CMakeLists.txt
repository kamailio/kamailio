file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

# rabbitmq provides cmake support for versions 0.13 and above.
# 0.11 provides cmake files but they are broken (report wrong version).
# https://github.com/alanxz/rabbitmq-c/commit/20bebf423bd3686fe73634ae239d09e5d21abde4
# Use pkg-config for all versions
# TODO: When we have a proper cmake config file for rabbitmq-c for most OS, we can
# use that instead of pkg-config.
find_package(PkgConfig REQUIRED)
pkg_check_modules(rabbitmq-c REQUIRED IMPORTED_TARGET librabbitmq)
add_library(rabbitmq::rabbitmq ALIAS PkgConfig::rabbitmq-c)

pkg_check_modules(UUID REQUIRED IMPORTED_TARGET uuid)
add_library(uuid::uuid ALIAS PkgConfig::UUID)

if(rabbitmq-c_VERSION VERSION_GREATER_EQUAL "0.13")
  target_compile_definitions(${module_name} PRIVATE RABBITMQ_DEPRECATION)
endif()

target_link_libraries(${module_name} PRIVATE rabbitmq::rabbitmq uuid::uuid)
