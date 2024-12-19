find_package(PkgConfig REQUIRED)
pkg_check_modules(libmosquitto REQUIRED IMPORTED_TARGET libmosquitto)
add_library(mosquitto::mosquitto ALIAS PkgConfig::libmosquitto)

find_package(Libev REQUIRED)

file(GLOB SRC_FILES "*.c")

add_library(mqtt SHARED ${SRC_FILES})
target_link_libraries(mqtt PRIVATE mosquitto::mosquitto Libev::Libev)
