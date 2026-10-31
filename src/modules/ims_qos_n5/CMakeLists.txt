file(GLOB MODULE_SOURCES "*.c")

add_library(${module_name} SHARED ${MODULE_SOURCES})

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)

target_link_libraries(${module_name} PRIVATE CURL::libcurl PkgConfig::JANSSON)
