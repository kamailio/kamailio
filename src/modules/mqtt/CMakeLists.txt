find_package(PkgConfig REQUIRED)
pkg_check_modules(libmosquitto REQUIRED IMPORTED_TARGET libmosquitto)
add_library(mosquitto::mosquitto ALIAS PkgConfig::libmosquitto)

find_package(Libev REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(mqtt SHARED ${SRC_FILES})
target_link_libraries(mqtt PRIVATE mosquitto::mosquitto Libev::Libev)

target_compile_options(
  mqtt PUBLIC "-ffile-prefix-map=${CMAKE_SOURCE_DIR}/src/modules/mqtt/=")
set_target_properties(mqtt PROPERTIES SUFFIX ".so")
