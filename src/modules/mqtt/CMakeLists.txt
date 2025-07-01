file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(libmosquitto REQUIRED IMPORTED_TARGET libmosquitto)
add_library(mosquitto::mosquitto ALIAS PkgConfig::libmosquitto)

find_package(Libev REQUIRED)

target_link_libraries(${module_name} PRIVATE mosquitto::mosquitto Libev::Libev)
